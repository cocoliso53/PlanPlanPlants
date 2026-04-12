package main

import (
	"database/sql"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"time"

	_ "modernc.org/sqlite"
)

type healthResponse struct {
	Status string `json:"status"`
}

type ResultReady = bool

type testingLogs struct {
	Moist1    int     `json:"moist1"`
	Temp      float64 `json:"temp"`
	Humidity  float64 `json:"humidity"`
	Lux       float64 `json:"lux"`
	Timestamp uint64  `json:"timestamp"`
}

type testingLogsAvg struct {
	AvgMoist1   float64 `json:"moist1"`
	AvgTemp     float64 `json:"temp"`
	AvgHumidity float64 `json:"humidity"`
	AvgLux      float64 `json:"lux"`
	Timestamp   int64   `json:"timestamp"`
}

type testingLogsSlice struct {
	s []testingLogs
}

type echoResponse struct {
	Status  string              `json:"status"`
	Params  map[string][]string `json:"params"`
	Payload testingLogs         `json:"payload"`
}

func main() {
	addr := ":8080"
	readings := testingLogsSlice{
		s: make([]testingLogs, 0, 5),
	}
	db, err := sql.Open("sqlite", "data/planplants.db")
	if err != nil {
		log.Fatal(err)
	}
	defer db.Close()

	if _, err := db.Exec(`
		CREATE TABLE IF NOT EXISTS average_readings (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			moist1 REAL NOT NULL,
			temp REAL NOT NULL,
			humidity REAL NOT NULL,
			lux REAL NOT NULL,
			timestamp INTEGER NOT NULL
		)
	`); err != nil {
		log.Fatal(err)
	}

	if port := os.Getenv("PORT"); port != "" {
		addr = ":" + port
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/", rootHandler)
	mux.HandleFunc("/health", healthHandler)
	mux.HandleFunc("/echo", echoHandler)
	mux.HandleFunc("/readings", func(w http.ResponseWriter, r *http.Request) {
		averageReadingsDataHandler(db, &readings, w, r)
	})

	server := &http.Server{
		Addr:    addr,
		Handler: loggingMiddleware(mux),
	}

	log.Printf("server listening on %s", addr)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatal(err)
	}
}

func rootHandler(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("PlanPlanPlants server\n"))
}

func echoHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	defer r.Body.Close()

	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, 1<<20))
	if err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	var payload testingLogs

	if err := json.Unmarshal(body, &payload); err != nil {
		http.Error(w, "invalid json body", http.StatusBadRequest)
		return
	}

	params := r.URL.Query()
	log.Printf(
		"echo request params=%v payload=%+v raw=%s",
		params,
		payload,
		string(body),
	)

	writeJSON(w, http.StatusOK, echoResponse{
		Status:  "ok",
		Params:  params,
		Payload: payload,
	})
}

func (r *testingLogsSlice) averageReadingData(latestReading testingLogs) (testingLogsAvg, ResultReady) {
	// let's take the average of 5 readings
	maxLen := 5.0

	r.s = append(r.s, latestReading)

	if len(r.s) == int(maxLen) {
		var avgMoist1 float64
		var avgTemp float64
		var avgHumidity float64
		var avgLux float64

		for _, item := range r.s {
			avgMoist1 += float64(item.Moist1)
			avgTemp += item.Temp
			avgHumidity += item.Humidity
			avgLux += item.Lux
		}

		// reset to empty slice
		r.s = r.s[:0]

		return testingLogsAvg{
			AvgMoist1:   avgMoist1 / maxLen,
			AvgTemp:     avgTemp / maxLen,
			AvgHumidity: avgHumidity / maxLen,
			AvgLux:      avgLux / maxLen,
			Timestamp:   time.Now().Unix(),
		}, true
	} else {
		return testingLogsAvg{}, false
	}
}

func averageReadingsDataHandler(db *sql.DB, readings *testingLogsSlice, w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}

	defer r.Body.Close()

	body, err := io.ReadAll(http.MaxBytesReader(w, r.Body, 1<<20))
	if err != nil {
		http.Error(w, "invalid request body", http.StatusBadRequest)
		return
	}

	var reading testingLogs

	if err := json.Unmarshal(body, &reading); err != nil {
		http.Error(w, "invalid json body", http.StatusBadRequest)
		return
	}

	result, ready := readings.averageReadingData(reading)
	if ready {
		if _, err := db.Exec(
			`INSERT INTO average_readings (moist1, temp, humidity, lux, timestamp) VALUES (?, ?, ?, ?, ?)`,
			result.AvgMoist1,
			result.AvgTemp,
			result.AvgHumidity,
			result.AvgLux,
			result.Timestamp,
		); err != nil {
			http.Error(w, "failed to store average reading", http.StatusInternalServerError)
			return
		}
		writeJSON(w, http.StatusCreated, result)
	} else {
		w.WriteHeader(http.StatusNoContent)
	}

}

func healthHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, http.StatusText(http.StatusMethodNotAllowed), http.StatusMethodNotAllowed)
		return
	}

	writeJSON(w, http.StatusOK, healthResponse{
		Status: "ok",
	})
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)

	if err := json.NewEncoder(w).Encode(value); err != nil {
		log.Printf("write json error: %v", err)
	}
}

func loggingMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		lrw := &loggingResponseWriter{
			ResponseWriter: w,
			statusCode:     http.StatusOK,
		}

		log.Printf("request started method=%s path=%s remote=%s", r.Method, r.URL.Path, r.RemoteAddr)
		next.ServeHTTP(lrw, r)
		log.Printf(
			"request completed method=%s path=%s status=%d bytes=%d duration=%s",
			r.Method,
			r.URL.Path,
			lrw.statusCode,
			lrw.bytesWritten,
			time.Since(start),
		)
	})
}

type loggingResponseWriter struct {
	http.ResponseWriter
	statusCode   int
	bytesWritten int
}

func (w *loggingResponseWriter) WriteHeader(statusCode int) {
	w.statusCode = statusCode
	w.ResponseWriter.WriteHeader(statusCode)
}

func (w *loggingResponseWriter) Write(data []byte) (int, error) {
	n, err := w.ResponseWriter.Write(data)
	w.bytesWritten += n
	return n, err
}
