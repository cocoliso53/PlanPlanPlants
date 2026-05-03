package main

import "testing"

func TestAverageReadingDataNotReadyBeforeFiveSamples(t *testing.T) {
	readings := &testingLogsSlice{}

	inputs := []testingLogs{
		{Moist1: 100, Moist2: 200, Temp: 20.0, Humidity: 40.0, Lux1: 1000.0, Lux2: 2000.0, Timestamp: 1},
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
		{Moist1: 100, Moist2: 200, Temp: 20.0, Humidity: 40.0, Lux1: 1000.0, Lux2: 2000.0, Timestamp: 1},
		{Moist1: 110, Moist2: 210, Temp: 22.0, Humidity: 42.0, Lux1: 1100.0, Lux2: 2100.0, Timestamp: 2},
		{Moist1: 120, Moist2: 220, Temp: 24.0, Humidity: 44.0, Lux1: 1200.0, Lux2: 2200.0, Timestamp: 3},
		{Moist1: 130, Moist2: 230, Temp: 26.0, Humidity: 46.0, Lux1: 1300.0, Lux2: 2300.0, Timestamp: 4},
		{Moist1: 140, Moist2: 240, Temp: 28.0, Humidity: 48.0, Lux1: 1400.0, Lux2: 2400.0, Timestamp: 5},
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
