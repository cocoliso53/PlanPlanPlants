package main

import (
	"bytes"
	"database/sql"
	"encoding/json"
	"fmt"
	"html"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	_ "modernc.org/sqlite"
)

type healthResponse struct {
	Status string `json:"status"`
}

type ResultReady = bool

const prototypeDeviceID = "prototype"

type testingLogs struct {
	Moist1       int     `json:"moist1"`
	Moist2       int     `json:"moist2"`
	Temp         float64 `json:"temp"`
	Humidity     float64 `json:"humidity"`
	Lux1         float64 `json:"lux1"`
	Lux2         float64 `json:"lux2"`
	BatteryVolts float64 `json:"batteryPinVoltage"`
	Timestamp    uint64  `json:"timestamp"`
}

type testingLogsAvg struct {
	AvgMoist1       float64 `json:"moist1"`
	AvgMoist2       float64 `json:"moist2"`
	AvgTemp         float64 `json:"temp"`
	AvgHumidity     float64 `json:"humidity"`
	AvgLux1         float64 `json:"lux1"`
	AvgLux2         float64 `json:"lux2"`
	AvgBatteryVolts float64 `json:"batteryPinVoltage"`
	Timestamp       int64   `json:"timestamp"`
}

type testingLogsSlice struct {
	s []testingLogs
}

type nodeSingleReading struct {
	Moisture   float64 `json:"moisture"`
	Lux        float64 `json:"lux"`
	BatteryRaw float64 `json:"batteryRaw"`
}

type nodeBatchReading struct {
	NodeId    float64             `json:"nodeId"`
	TimeStamp float64             `json:"timestamp"`
	Readings  []nodeSingleReading `json:"readings"`
}

type nodeData struct {
	Data []nodeBatchReading `json:"data"`
}

type avgNodeReading struct {
	AvgMoisture   float64
	AvgLux        float64
	AvgBatteryRaw float64
}

type nodeRow struct {
	avgNodeReading
	TimeStamp float64
	NodeId    float64
}

type echoResponse struct {
	Status  string              `json:"status"`
	Params  map[string][]string `json:"params"`
	Payload json.RawMessage     `json:"payload"`
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

	if err := ensureAverageReadingsTable(db); err != nil {
		log.Fatal(err)
	}

	if token := os.Getenv("TELEGRAM_BOT_TOKEN"); token != "" {
		go startTelegramBot(db, token)
	}

	if port := os.Getenv("PORT"); port != "" {
		addr = ":" + port
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/", rootHandler)
	mux.HandleFunc("/health", healthHandler)
	mux.HandleFunc("/echo", echoHandler)
	mux.HandleFunc("/readings", func(w http.ResponseWriter, r *http.Request) {
		averageReadingsDataHandler(db, w, r)
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

	var payload json.RawMessage

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

func averageBatchReadings(batchReading []nodeSingleReading) (avgNodeReading, ResultReady) {
	var sumMoist float64
	var sumLux float64
	var sumBatteryRaw float64
	var lenReadings = len(batchReading)

	if lenReadings > 0 {
		for _, item := range batchReading {
			sumMoist += item.Moisture
			sumLux += item.Lux
			sumBatteryRaw += item.BatteryRaw
		}

		return avgNodeReading{
			AvgMoisture:   sumMoist / float64(lenReadings),
			AvgLux:        sumLux / float64(lenReadings),
			AvgBatteryRaw: sumBatteryRaw / float64(lenReadings),
		}, true
	} else {
		return avgNodeReading{}, false
	}
}

func averageReadingsDataHandler(db *sql.DB, w http.ResponseWriter, r *http.Request) {
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

	var batches nodeData

	if err := json.Unmarshal(body, &batches); err != nil {
		http.Error(w, "invalid json body", http.StatusBadRequest)
		return
	}

	for _, item := range batches.Data {
		result, ready := averageBatchReadings(item.Readings)
		if ready {
			if _, err := db.Exec(
				`INSERT INTO average_readings (nodeId, timestamp, moisture, lux, batteryPinVoltage) VALUES (?, ?, ?, ?, ?)`,
				item.NodeId,
				item.TimeStamp,
				result.AvgMoisture,
				result.AvgLux,
				result.AvgBatteryRaw,
			); err != nil {
				http.Error(w, "failed to store average reading", http.StatusInternalServerError)
				return
			}
			writeJSON(w, http.StatusCreated,
				nodeRow{
					result,
					item.NodeId,
					item.TimeStamp,
				},
			)
		} else {
			w.WriteHeader(http.StatusNoContent)
		}
	}
}

func ensureAverageReadingsTable(db *sql.DB) error {
	if _, err := db.Exec(`
		CREATE TABLE IF NOT EXISTS average_readings (
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
		return err
	}

	rows, err := db.Query(`PRAGMA table_info(average_readings)`)
	if err != nil {
		return err
	}
	defer rows.Close()

	columns := make(map[string]bool)
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
			return err
		}
		columns[name] = true
	}

	if err := rows.Err(); err != nil {
		return err
	}

	if !columns["moist2"] {
		if _, err := db.Exec(`ALTER TABLE average_readings ADD COLUMN moist2 REAL NOT NULL DEFAULT 0`); err != nil {
			return err
		}
	}
	if !columns["lux1"] {
		if _, err := db.Exec(`ALTER TABLE average_readings ADD COLUMN lux1 REAL NOT NULL DEFAULT 0`); err != nil {
			return err
		}
	}
	if !columns["lux2"] {
		if _, err := db.Exec(`ALTER TABLE average_readings ADD COLUMN lux2 REAL NOT NULL DEFAULT 0`); err != nil {
			return err
		}
	}

	if !columns["batteryPinVoltage"] {
		if _, err := db.Exec(`ALTER TABLE average_readings ADD COLUMN batteryPinVoltage REAL NOT NULL DEFAULT 0`); err != nil {
			return err
		}
	}

	if !columns["deviceId"] {
		if _, err := db.Exec(`ALTER TABLE average_readings ADD COLUMN deviceId TEXT NOT NULL DEFAULT 'prototype'`); err != nil {
			return err
		}
	}

	return nil
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

// Telegram bot

type telegramUpdatesResponse struct {
	OK     bool             `json:"ok"`
	Result []telegramUpdate `json:"result"`
}

type telegramUpdate struct {
	UpdateID int             `json:"update_id"`
	Message  telegramMessage `json:"message"`
}

type telegramMessage struct {
	Chat telegramChat `json:"chat"`
	Text string       `json:"text"`
}

type telegramChat struct {
	ID int64 `json:"id"`
}

func startTelegramBot(db *sql.DB, token string) {
	client := &http.Client{Timeout: 35 * time.Second}
	baseURL := "https://api.telegram.org/bot" + token
	offset := 0

	log.Println("telegram bot started")

	for {
		updates, err := getTelegramUpdates(client, baseURL, offset)
		if err != nil {
			log.Printf("telegram getUpdates error: %v", err)
			time.Sleep(3 * time.Second)
			continue
		}

		for _, update := range updates {
			if update.UpdateID >= offset {
				offset = update.UpdateID + 1
			}
			if update.Message.Chat.ID == 0 || update.Message.Text == "" {
				continue
			}

			message := telegramCommandResponse(db, update.Message.Text)
			if message == "" {
				continue
			}
			if err := sendTelegramMessage(client, baseURL, update.Message.Chat.ID, message); err != nil {
				log.Printf("telegram sendMessage error: %v", err)
			}
		}
	}
}

func telegramCommandResponse(db *sql.DB, text string) string {
	fields := strings.Fields(text)
	if len(fields) == 0 {
		return ""
	}

	command := strings.ToLower(fields[0])
	command = strings.Split(command, "@")[0]

	switch command {
	case "/latets", "/latest":
		message, err := latestReadingsMessage(db, 5)
		if err != nil {
			return "failed to read latest readings: " + err.Error()
		}
		return message
	case "/start", "/help":
		return "Available commands:\n/latets - show latest 5 readings"
	default:
		return ""
	}
}

func latestReadingsMessage(db *sql.DB, limit int) (string, error) {
	rows, err := db.Query("SELECT * FROM average_readings ORDER BY timestamp DESC LIMIT ?", limit)
	if err != nil {
		return "", err
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return "", err
	}

	var values [][]string
	for rows.Next() {
		raw := make([]any, len(columns))
		dest := make([]any, len(columns))
		for i := range raw {
			dest[i] = &raw[i]
		}
		if err := rows.Scan(dest...); err != nil {
			return "", err
		}

		row := make([]string, len(columns))
		for i, value := range raw {
			row[i] = sqliteValueString(value)
		}
		values = append(values, row)
	}
	if err := rows.Err(); err != nil {
		return "", err
	}
	if len(values) == 0 {
		return "no readings found", nil
	}

	return formatReadingsByMeasurementHTML(columns, values), nil
}

func sqliteValueString(value any) string {
	switch v := value.(type) {
	case nil:
		return ""
	case []byte:
		return string(v)
	default:
		return fmt.Sprint(v)
	}
}

func formatReadingsByMeasurementHTML(columns []string, rows [][]string) string {
	var out strings.Builder

	for columnIndex, column := range columns {
		if column == "id" {
			continue
		}
		if out.Len() > 0 {
			out.WriteString("\n\n")
		}

		out.WriteString("<b>")
		out.WriteString(html.EscapeString(column))
		out.WriteString("</b>: ")

		for rowIndex := len(rows) - 1; rowIndex >= 0; rowIndex-- {
			if rowIndex != len(rows)-1 {
				out.WriteString(", ")
			}
			out.WriteString(html.EscapeString(rows[rowIndex][columnIndex]))
		}
	}

	return out.String()
}

func getTelegramUpdates(client *http.Client, baseURL string, offset int) ([]telegramUpdate, error) {
	url := baseURL + "/getUpdates?timeout=30&offset=" + strconv.Itoa(offset)
	resp, err := client.Get(url)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("status %d: %s", resp.StatusCode, string(body))
	}

	var decoded telegramUpdatesResponse
	if err := json.Unmarshal(body, &decoded); err != nil {
		return nil, err
	}
	if !decoded.OK {
		return nil, fmt.Errorf("telegram returned ok=false")
	}

	return decoded.Result, nil
}

func sendTelegramMessage(client *http.Client, baseURL string, chatID int64, text string) error {
	payload := map[string]any{
		"chat_id":    chatID,
		"text":       text,
		"parse_mode": "HTML",
	}
	encoded, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	resp, err := client.Post(baseURL+"/sendMessage", "application/json", bytes.NewReader(encoded))
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("status %d: %s", resp.StatusCode, string(body))
	}

	return nil
}
