package main

import "testing"

func TestAverageReadingDataNotReadyBeforeFiveSamples(t *testing.T) {
	readings := &testingLogsSlice{}

	inputs := []testingLogs{
		{Moist1: 100, Temp: 20.0, Humidity: 40.0, Lux: 1000.0, Timestamp: 1},
		{Moist1: 110, Temp: 21.0, Humidity: 41.0, Lux: 1010.0, Timestamp: 2},
		{Moist1: 120, Temp: 22.0, Humidity: 42.0, Lux: 1020.0, Timestamp: 3},
		{Moist1: 130, Temp: 23.0, Humidity: 43.0, Lux: 1030.0, Timestamp: 4},
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
		{Moist1: 100, Temp: 20.0, Humidity: 40.0, Lux: 1000.0, Timestamp: 1},
		{Moist1: 110, Temp: 22.0, Humidity: 42.0, Lux: 1100.0, Timestamp: 2},
		{Moist1: 120, Temp: 24.0, Humidity: 44.0, Lux: 1200.0, Timestamp: 3},
		{Moist1: 130, Temp: 26.0, Humidity: 46.0, Lux: 1300.0, Timestamp: 4},
		{Moist1: 140, Temp: 28.0, Humidity: 48.0, Lux: 1400.0, Timestamp: 5},
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
	if got.AvgTemp != 24 {
		t.Fatalf("AvgTemp = %v, want 24", got.AvgTemp)
	}
	if got.AvgHumidity != 44 {
		t.Fatalf("AvgHumidity = %v, want 44", got.AvgHumidity)
	}
	if got.AvgLux != 1200 {
		t.Fatalf("AvgLux = %v, want 1200", got.AvgLux)
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
		{Moist1: 10, Temp: 1, Humidity: 11, Lux: 101, Timestamp: 1},
		{Moist1: 20, Temp: 2, Humidity: 12, Lux: 102, Timestamp: 2},
		{Moist1: 30, Temp: 3, Humidity: 13, Lux: 103, Timestamp: 3},
		{Moist1: 40, Temp: 4, Humidity: 14, Lux: 104, Timestamp: 4},
		{Moist1: 50, Temp: 5, Humidity: 15, Lux: 105, Timestamp: 5},
	}

	for _, input := range firstWindow {
		readings.averageReadingData(input)
	}

	got, ready := readings.averageReadingData(testingLogs{
		Moist1: 200, Temp: 30, Humidity: 60, Lux: 900, Timestamp: 6,
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
