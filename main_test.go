package main

import (
	"database/sql"
	"math"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
)

func TestAverageReadingDataNotReadyBeforeFiveSamples(t *testing.T) {
	readings := &testingLogsSlice{}

	inputs := []testingLogs{
		{Moist1: 100, Moist2: 200, Temp: 20.0, Humidity: 40.0, Lux1: 1000.0, Lux2: 2000.0, BatteryVolts: 1.8, Timestamp: 1},
		{Moist1: 110, Moist2: 210, Temp: 21.0, Humidity: 41.0, Lux1: 1010.0, Lux2: 2010.0, Timestamp: 2},
		{Moist1: 120, Moist2: 220, Temp: 22.0, Humidity: 42.0, Lux1: 1020.0, Lux2: 2020.0, Timestamp: 3},
		{Moist1: 130, Moist2: 230, Temp: 23.0, Humidity: 43.0, Lux1: 1030.0, Lux2: 2030.0, Timestamp: 4},
	}

	for i, input := range inputs {
		got, ready := readings.averageReadingData(input)
		if ready {
			t.Fatalf("call %d: ready = true, want false", i+1)
		}
		if got != (testingLogsAvg{}) {
			t.Fatalf("call %d: got %#v, want zero-value result", i+1, got)
		}
	}

	if gotLen := len(readings.s); gotLen != 4 {
		t.Fatalf("slice length = %d, want 4", gotLen)
	}
}

func TestAverageReadingDataReturnsAverageOnFifthSampleAndResets(t *testing.T) {
	readings := &testingLogsSlice{}

	inputs := []testingLogs{
		{Moist1: 100, Moist2: 200, Temp: 20.0, Humidity: 40.0, Lux1: 1000.0, Lux2: 2000.0, BatteryVolts: 1.8, Timestamp: 1},
		{Moist1: 110, Moist2: 210, Temp: 22.0, Humidity: 42.0, Lux1: 1100.0, Lux2: 2100.0, BatteryVolts: 1.9, Timestamp: 2},
		{Moist1: 120, Moist2: 220, Temp: 24.0, Humidity: 44.0, Lux1: 1200.0, Lux2: 2200.0, BatteryVolts: 2.0, Timestamp: 3},
		{Moist1: 130, Moist2: 230, Temp: 26.0, Humidity: 46.0, Lux1: 1300.0, Lux2: 2300.0, BatteryVolts: 2.1, Timestamp: 4},
		{Moist1: 140, Moist2: 240, Temp: 28.0, Humidity: 48.0, Lux1: 1400.0, Lux2: 2400.0, BatteryVolts: 2.2, Timestamp: 5},
	}

	var got testingLogsAvg
	var ready bool
	for _, input := range inputs {
		got, ready = readings.averageReadingData(input)
	}

	if !ready {
		t.Fatal("ready = false, want true on fifth sample")
	}

	if got.AvgMoist1 != 120 {
		t.Fatalf("AvgMoist1 = %v, want 120", got.AvgMoist1)
	}
	if got.AvgMoist2 != 220 {
		t.Fatalf("AvgMoist2 = %v, want 220", got.AvgMoist2)
	}
	if got.AvgTemp != 24 {
		t.Fatalf("AvgTemp = %v, want 24", got.AvgTemp)
	}
	if got.AvgHumidity != 44 {
		t.Fatalf("AvgHumidity = %v, want 44", got.AvgHumidity)
	}
	if got.AvgLux1 != 1200 {
		t.Fatalf("AvgLux1 = %v, want 1200", got.AvgLux1)
	}
	if got.AvgLux2 != 2200 {
		t.Fatalf("AvgLux2 = %v, want 2200", got.AvgLux2)
	}
	if got.AvgBatteryVolts != 2.0 {
		t.Fatalf("AvgBatteryVolts = %v, want 2.0", got.AvgBatteryVolts)
	}

	if got.Timestamp <= 0 {
		t.Fatalf("Timestamp = %d, want positive Unix timestamp", got.Timestamp)
	}

	if gotLen := len(readings.s); gotLen != 0 {
		t.Fatalf("slice length after reset = %d, want 0", gotLen)
	}
}

func TestAverageReadingDataStartsNewWindowAfterReset(t *testing.T) {
	readings := &testingLogsSlice{}

	firstWindow := []testingLogs{
		{Moist1: 10, Moist2: 20, Temp: 1, Humidity: 11, Lux1: 101, Lux2: 201, Timestamp: 1},
		{Moist1: 20, Moist2: 30, Temp: 2, Humidity: 12, Lux1: 102, Lux2: 202, Timestamp: 2},
		{Moist1: 30, Moist2: 40, Temp: 3, Humidity: 13, Lux1: 103, Lux2: 203, Timestamp: 3},
		{Moist1: 40, Moist2: 50, Temp: 4, Humidity: 14, Lux1: 104, Lux2: 204, Timestamp: 4},
		{Moist1: 50, Moist2: 60, Temp: 5, Humidity: 15, Lux1: 105, Lux2: 205, Timestamp: 5},
	}

	for _, input := range firstWindow {
		readings.averageReadingData(input)
	}

	got, ready := readings.averageReadingData(testingLogs{
		Moist1: 200, Moist2: 300, Temp: 30, Humidity: 60, Lux1: 900, Lux2: 1900, Timestamp: 6,
	})
	if ready {
		t.Fatal("ready = true, want false for first sample of new window")
	}
	if got != (testingLogsAvg{}) {
		t.Fatalf("got %#v, want zero-value result", got)
	}
	if gotLen := len(readings.s); gotLen != 1 {
		t.Fatalf("slice length = %d, want 1 after starting new window", gotLen)
	}
}

func TestEnsureAverageReadingsTableAddsBatteryPinVoltageColumn(t *testing.T) {
	db, err := sql.Open("sqlite", filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()

	if _, err := db.Exec(`
		CREATE TABLE average_readings (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			moist1 REAL NOT NULL,
			moist2 REAL NOT NULL DEFAULT 0,
			temp REAL NOT NULL,
			humidity REAL NOT NULL,
			lux1 REAL NOT NULL DEFAULT 0,
			lux2 REAL NOT NULL DEFAULT 0,
			deviceId TEXT NOT NULL DEFAULT 'prototype',
			timestamp INTEGER NOT NULL
		)
	`); err != nil {
		t.Fatal(err)
	}

	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	var count int
	if err := db.QueryRow(
		`SELECT COUNT(*) FROM pragma_table_info('average_readings') WHERE name = 'batteryPinVoltage'`,
	).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 1 {
		t.Fatalf("batteryPinVoltage column count = %d, want 1", count)
	}
}

func TestAverageReadingsDataHandlerStoresBatteryPinVoltage(t *testing.T) {
	db, err := sql.Open("sqlite", filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()

	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	readings := &testingLogsSlice{}
	payload := `{"moist1":100,"moist2":200,"temp":24.5,"humidity":55,"lux1":400,"lux2":450,"batteryPinVoltage":1.95,"timestamp":1}`

	for i := 1; i <= 5; i++ {
		req := httptest.NewRequest(http.MethodPost, "/readings", strings.NewReader(payload))
		recorder := httptest.NewRecorder()

		averageReadingsDataHandler(db, readings, recorder, req)

		wantStatus := http.StatusNoContent
		if i == 5 {
			wantStatus = http.StatusCreated
		}
		if recorder.Code != wantStatus {
			t.Fatalf("request %d status = %d, want %d", i, recorder.Code, wantStatus)
		}
	}

	var batteryPinVoltage float64
	var deviceID string
	if err := db.QueryRow(
		`SELECT batteryPinVoltage, deviceId FROM average_readings`,
	).Scan(&batteryPinVoltage, &deviceID); err != nil {
		t.Fatal(err)
	}

	if math.Abs(batteryPinVoltage-1.95) > 0.0001 {
		t.Fatalf("batteryPinVoltage = %v, want 1.95", batteryPinVoltage)
	}
	if deviceID != prototypeDeviceID {
		t.Fatalf("deviceId = %q, want %q", deviceID, prototypeDeviceID)
	}
}
