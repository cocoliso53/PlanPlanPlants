package main

import (
	"database/sql"
	"encoding/json"
	"math"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
)

func TestAverageBatchReadingsReturnsAverage(t *testing.T) {
	got, ready := averageBatchReadings([]nodeSingleReading{
		{Moisture: 100, Lux: 10, BatteryRaw: 1000},
		{Moisture: 110, Lux: 20, BatteryRaw: 1010},
		{Moisture: 120, Lux: 30, BatteryRaw: 1020},
		{Moisture: 130, Lux: 40, BatteryRaw: 1030},
		{Moisture: 140, Lux: 50, BatteryRaw: 1040},
	})

	if !ready {
		t.Fatal("ready = false, want true")
	}
	assertFloatEqual(t, got.AvgMoisture, 120)
	assertFloatEqual(t, got.AvgLux, 30)
	assertFloatEqual(t, got.AvgBatteryRaw, 1020)
}

func TestAverageBatchReadingsNotReadyForEmptyBatch(t *testing.T) {
	got, ready := averageBatchReadings(nil)

	if ready {
		t.Fatal("ready = true, want false")
	}
	if got != (avgNodeReading{}) {
		t.Fatalf("got %#v, want zero-value result", got)
	}
}

func TestEnsureAverageReadingsTableResetsOldSchema(t *testing.T) {
	db := openTestDB(t)

	if _, err := db.Exec(`
		CREATE TABLE average_readings (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			moist1 REAL NOT NULL,
			moist2 REAL NOT NULL DEFAULT 0,
			temp REAL NOT NULL,
			humidity REAL NOT NULL,
			lux1 REAL NOT NULL DEFAULT 0,
			lux2 REAL NOT NULL DEFAULT 0,
			batteryPinVoltage REAL NOT NULL DEFAULT 0,
			deviceId TEXT NOT NULL DEFAULT 'prototype',
			timestamp INTEGER NOT NULL
		)
	`); err != nil {
		t.Fatal(err)
	}

	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	columns := tableColumns(t, db, "average_readings")
	wantColumns := []string{"id", "nodeId", "timestamp", "moisture", "lux", "batteryRaw"}
	if strings.Join(columns, ",") != strings.Join(wantColumns, ",") {
		t.Fatalf("columns = %v, want %v", columns, wantColumns)
	}
}

func TestEnsureAverageReadingsTablePreservesNewSchemaRows(t *testing.T) {
	db := openTestDB(t)

	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}
	if _, err := db.Exec(
		`INSERT INTO average_readings (nodeId, timestamp, moisture, lux, batteryRaw) VALUES (?, ?, ?, ?, ?)`,
		1,
		1000,
		100,
		20,
		900,
	); err != nil {
		t.Fatal(err)
	}

	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	var count int
	if err := db.QueryRow(`SELECT COUNT(*) FROM average_readings`).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 1 {
		t.Fatalf("row count = %d, want 1", count)
	}
}

func TestAverageReadingsDataHandlerStoresBatchAverages(t *testing.T) {
	db := openTestDB(t)
	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	payload := `{"data":[{"nodeId":1,"timestamp":2000,"readings":[{"moisture":100,"lux":10,"batteryRaw":1000},{"moisture":120,"lux":30,"batteryRaw":1040}]},{"nodeId":2,"timestamp":3000,"readings":[{"moisture":300,"lux":50,"batteryRaw":2000},{"moisture":500,"lux":70,"batteryRaw":2200}]}]}`
	req := httptest.NewRequest(http.MethodPost, "/readings", strings.NewReader(payload))
	recorder := httptest.NewRecorder()

	averageReadingsDataHandler(db, recorder, req)

	if recorder.Code != http.StatusCreated {
		t.Fatalf("status = %d, want %d; body=%s", recorder.Code, http.StatusCreated, recorder.Body.String())
	}

	var response struct {
		Stored []nodeRow `json:"stored"`
	}
	if err := json.Unmarshal(recorder.Body.Bytes(), &response); err != nil {
		t.Fatal(err)
	}
	if len(response.Stored) != 2 {
		t.Fatalf("stored response length = %d, want 2", len(response.Stored))
	}

	rows, err := db.Query(`SELECT nodeId, timestamp, moisture, lux, batteryRaw FROM average_readings ORDER BY nodeId`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()

	var stored []nodeRow
	for rows.Next() {
		var row nodeRow
		if err := rows.Scan(&row.NodeID, &row.Timestamp, &row.Moisture, &row.Lux, &row.BatteryRaw); err != nil {
			t.Fatal(err)
		}
		stored = append(stored, row)
	}
	if err := rows.Err(); err != nil {
		t.Fatal(err)
	}

	if len(stored) != 2 {
		t.Fatalf("stored row count = %d, want 2", len(stored))
	}
	assertFloatEqual(t, stored[0].Moisture, 110)
	assertFloatEqual(t, stored[0].Lux, 20)
	assertFloatEqual(t, stored[0].BatteryRaw, 1020)
	assertFloatEqual(t, stored[1].Moisture, 400)
	assertFloatEqual(t, stored[1].Lux, 60)
	assertFloatEqual(t, stored[1].BatteryRaw, 2100)
}

func TestAverageReadingsDataHandlerAcceptsEmptyHeartbeat(t *testing.T) {
	db := openTestDB(t)
	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	req := httptest.NewRequest(http.MethodPost, "/readings", strings.NewReader(`{"data":[]}`))
	recorder := httptest.NewRecorder()

	averageReadingsDataHandler(db, recorder, req)

	if recorder.Code != http.StatusNoContent {
		t.Fatalf("status = %d, want %d", recorder.Code, http.StatusNoContent)
	}

	var count int
	if err := db.QueryRow(`SELECT COUNT(*) FROM average_readings`).Scan(&count); err != nil {
		t.Fatal(err)
	}
	if count != 0 {
		t.Fatalf("row count = %d, want 0", count)
	}
}

func TestTelegramLatetsCommandReturnsLatestReadings(t *testing.T) {
	db := openTestDB(t)
	if err := ensureAverageReadingsTable(db); err != nil {
		t.Fatal(err)
	}

	for i := 1; i <= 6; i++ {
		if _, err := db.Exec(
			`INSERT INTO average_readings (nodeId, timestamp, moisture, lux, batteryRaw) VALUES (?, ?, ?, ?, ?)`,
			1,
			1000+i,
			100+i,
			10+i,
			900+i,
		); err != nil {
			t.Fatal(err)
		}
	}

	got := telegramCommandResponse(db, "/latets")
	if !strings.Contains(got, "<b>moisture</b>: 102, 103, 104, 105, 106") {
		t.Fatalf("unexpected /latets response:\n%s", got)
	}
	if !strings.Contains(got, "<b>timestamp</b>: 1002, 1003, 1004, 1005, 1006") {
		t.Fatalf("unexpected timestamp order in /latets response:\n%s", got)
	}
	if strings.Contains(got, "1001") || strings.Contains(got, "<b>id</b>") {
		t.Fatalf("/latets response should only include latest 5 rows and should skip id:\n%s", got)
	}
}

func openTestDB(t *testing.T) *sql.DB {
	t.Helper()

	db, err := sql.Open("sqlite", filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = db.Close() })
	return db
}

func tableColumns(t *testing.T, db *sql.DB, table string) []string {
	t.Helper()

	rows, err := db.Query(`PRAGMA table_info(` + table + `)`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()

	var columns []string
	for rows.Next() {
		var (
			cid       int
			name      string
			fieldType string
			notNull   int
			defaultV  sql.NullString
			pk        int
		)
		if err := rows.Scan(&cid, &name, &fieldType, &notNull, &defaultV, &pk); err != nil {
			t.Fatal(err)
		}
		columns = append(columns, name)
	}
	if err := rows.Err(); err != nil {
		t.Fatal(err)
	}
	return columns
}

func assertFloatEqual(t *testing.T, got, want float64) {
	t.Helper()

	if math.Abs(got-want) > 0.0001 {
		t.Fatalf("got %v, want %v", got, want)
	}
}
