package main

import (
	"encoding/csv"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"sync"
	"time"
)

type healthResponse struct {
	Status string `json:"status"`
}

type createPlantRequest struct {
	Name  string `json:"name"`
	Water string `json:"water"`
}

type createPlantResponse struct {
	ID      int    `json:"id"`
	Name    string `json:"name"`
	Water   string `json:"water"`
	Message string `json:"message"`
}

type testingLogs struct {
	SensorLabel string `json:"sensorLabel"`
	Value       int    `json:"value"`
	Timestamp   uint64 `json:"timestamp"`
}

var csvMu sync.Mutex

func main() {
	addr := ":8080"
	if port := os.Getenv("PORT"); port != "" {
		addr = ":" + port
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/", rootHandler)
	mux.HandleFunc("/health", healthHandler)
	mux.HandleFunc("/plants", createPlantHandler)
	mux.HandleFunc("/echo", echoHandler)

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

	var payload testingLogs

	err := json.NewDecoder(r.Body).Decode(&payload)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return
	}

	if err := appendTestingLogCSV(payload); err != nil {
		log.Printf("csv write error: %v", err)
		http.Error(w, "failed to store log", http.StatusInternalServerError)
		return
	}

	writeJSON(w, http.StatusOK, healthResponse{
		Status: "ok",
	})
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

func createPlantHandler(w http.ResponseWriter, r *http.Request) {
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

	var req createPlantRequest
	if err := json.Unmarshal(body, &req); err != nil {
		http.Error(w, "invalid json body", http.StatusBadRequest)
		return
	}

	if req.Name == "" {
		http.Error(w, "name is required", http.StatusBadRequest)
		return
	}

	writeJSON(w, http.StatusCreated, createPlantResponse{
		ID:      1,
		Name:    req.Name,
		Water:   req.Water,
		Message: "plant created",
	})
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)

	if err := json.NewEncoder(w).Encode(value); err != nil {
		log.Printf("write json error: %v", err)
	}
}

func appendTestingLogCSV(entry testingLogs) error {
	csvMu.Lock()
	defer csvMu.Unlock()

	const csvPath = "data/calibration_soil_sensor_logs_dry_soil1.csv"

	if err := os.MkdirAll(filepath.Dir(csvPath), 0o755); err != nil {
		return err
	}

	file, err := os.OpenFile(csvPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return err
	}
	defer file.Close()

	info, err := file.Stat()
	if err != nil {
		return err
	}

	writer := csv.NewWriter(file)
	if info.Size() == 0 {
		if err := writer.Write([]string{"sensor_label", "value", "timestamp"}); err != nil {
			return err
		}
	}

	record := []string{
		entry.SensorLabel,
		strconv.Itoa(entry.Value),
		strconv.FormatUint(entry.Timestamp, 10),
	}

	if err := writer.Write(record); err != nil {
		return err
	}

	writer.Flush()
	return writer.Error()
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
