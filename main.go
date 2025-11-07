package main

import (
	"bytes"
	"encoding/csv"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

type rootHandler struct {
	df *os.File
}

func filterRow(timestampInRecord string, atTimestamp int64) bool {
	timestampRecordInt, _ := strconv.ParseInt(timestampInRecord, 10, 64)
	if timestampRecordInt >= atTimestamp {
		return true
	}
	return false
}

func getHistory(df *os.File, atTimestamp int64) float64 {
	var atTimestampRecord []string
	var atTimestampIndexInt int64
	df.Seek(0, 0)
	content, _ := io.ReadAll(df)
	r := csv.NewReader(bytes.NewReader(content))
	records, err := r.ReadAll()
	if err != nil {
		log.Fatal(err)
	}
	for _, record := range records {
		if filterRow(record[0], atTimestamp) {
			atTimestampRecord = record
			break
		}
	}
	lastRecord := records[len(records)-1]
	if atTimestampRecord != nil {
		aDayAgoIndexStr := strings.Split(atTimestampRecord[1], " ")[1]
		fmt.Println(aDayAgoIndexStr)
		atTimestampIndexInt, _ = strconv.ParseInt(aDayAgoIndexStr, 10, 64)
		fmt.Println("atTimestampIndexInt", atTimestampIndexInt)
		lastIndexStr := strings.Split(lastRecord[1], " ")[1]
		fmt.Println(lastIndexStr)
		lastIndexInt, _ := strconv.ParseInt(lastIndexStr, 10, 64)
		fmt.Println("lastIndexInt", lastIndexInt)
		deltaInt := lastIndexInt - atTimestampIndexInt
		fmt.Printf("Delta: %d\n", deltaInt)
		return float64(deltaInt) / 1000.0
	}
	return 0
}

func getLastTimestamp(df *os.File) int64 {
	df.Seek(-40, 2)
	content, _ := io.ReadAll(df)
	stringContent := string(content)
	fmt.Println(stringContent)
	stringsList := strings.Split(stringContent, "\n")
	fmt.Println(stringsList)
	last := stringsList[len(stringsList)-2]
	fmt.Println(last)
	lastTimestampStr := strings.Split(last, ",")
	lastTimestampInt, _ := strconv.ParseInt(lastTimestampStr[0], 10, 64)
	fmt.Println(lastTimestampInt)
	return lastTimestampInt

}

func (h rootHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("got / request\n")
	switch r.Method {
	case http.MethodGet:
		aDayAgoUnix := time.Now().Add(-24 * time.Hour).Unix()
		deltaDay := getHistory(h.df, aDayAgoUnix)
		io.WriteString(w, fmt.Sprintf("Consommation sur les 24 dernières heures: %.3f kWh\n", deltaDay))
		aHourAgoUnix := time.Now().Add(-time.Hour).Unix()
		deltaHour := getHistory(h.df, aHourAgoUnix)
		io.WriteString(w, fmt.Sprintf("Consommation sur la dernière heure: %.3f kWh\n", deltaHour))
		fiveMinuteAgoUnix := time.Now().Add(-5 * time.Minute).Unix()
		delta5Minute := getHistory(h.df, fiveMinuteAgoUnix)
		io.WriteString(w, fmt.Sprintf("Consommation sur les 5 dernières minutes: %.3f kWh\n", delta5Minute))
		power5Minute := delta5Minute * 1000 * 12
		io.WriteString(w, fmt.Sprintf("Puissance estimée sur les 5 dernières minutes: %.0f W\n", power5Minute))
		lastTimestampInt := getLastTimestamp(h.df)
		lastTimestampUTC := time.Unix(lastTimestampInt, 0).UTC()
		io.WriteString(w, "\n")
		io.WriteString(w, fmt.Sprintf("Dernière mise à jour: %s\n", lastTimestampUTC))

	case http.MethodPost:
		body, err := io.ReadAll(r.Body)
		if err != nil {
			fmt.Printf("could not read body: %s\n", err)
		}
		fmt.Printf("body: %s\n", body)
		now := time.Now().Unix()
		content := fmt.Sprint(now) + "," + string(body)
		h.df.WriteString(content)
		h.df.WriteString(string('\n'))
		h.df.Sync()
	}
}

func main() {
	dataFileDescW, err := os.OpenFile("linky_data.txt", os.O_APPEND|os.O_CREATE|os.O_RDWR, 0644)
	if err != nil {
		log.Fatal(err)
		os.Exit(1)
	}
	roothandler := rootHandler{
		df: dataFileDescW,
	}
	http.Handle("/xzqd23/", roothandler)

	err = http.ListenAndServe(":8080", nil)
	if errors.Is(err, http.ErrServerClosed) {
		fmt.Printf("server closed\n")
	} else if err != nil {
		fmt.Printf("error starting server: %s\n", err)
		os.Exit(1)
	}
}
