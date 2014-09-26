package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"time"

	"./sphincter"
)

const (
	// Http GET actions
	ACN_OPEN  = "open"
	ACN_CLOSE = "close"
	ACN_STATE = "state"

	HASH_FILE = "./hashes.json"
)

// AuthWorker handles authentication through tokens.
type AuthWorker struct {
	HashFile         string
	FileLastModified time.Time

	Salt      string
	HashTable []struct {
		Mail    string
		Hash    string
		Enabled bool
	}
}

// Read and parse hash file if it has changed since last read
func (a *AuthWorker) ReadHashFile() error {

	info, err := os.Stat(a.HashFile)
	if err != nil {
		return err
	}

	// check whether file was changed since last read
	if !a.FileLastModified.Equal(info.ModTime()) {

		log.Println("reading hash file " + a.HashFile + " ...")

		content, err := ioutil.ReadFile(a.HashFile)
		if err != nil {
			return err
		}

		if err = json.Unmarshal(content, a); err != nil {
			return err
		}

		a.FileLastModified = info.ModTime()
	}

	return nil
}

// Check authentication for a given token
func (a *AuthWorker) Auth(token string) bool {
	// update hashtable
	if err := a.ReadHashFile(); err != nil {
		log.Fatal(err)
	}

	// compute salted hash from token
	h256 := sha256.New()
	io.WriteString(h256, token)
	io.WriteString(h256, a.Salt)
	chash := hex.EncodeToString(h256.Sum(nil))

	// check if computed hash matches any hash from table
	for _, entry := range a.HashTable {
		if entry.Enabled && chash == entry.Hash {
			log.Println("user authenticated: " + entry.Mail)
			return true
		}
	}
	log.Println("authentication denied for token: \"" + token + "\"")
	return false
}

type HttpHandler struct {
	sphincter    *Sphincter
	auth         *AuthWorker
	ResponseChan chan string
}

func (h HttpHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {

	if err := r.ParseForm(); err != nil {
		log.Println(err)
	}
	action := r.Form.Get("action")
	token := r.Form.Get("token")

	// need auth
	if (action == ACN_OPEN || action == ACN_CLOSE) && !h.auth.Auth(token) {
		fmt.Fprint(w, "NOT ALLOWED")
		return
	}

	var err error

	switch action {
	case ACN_OPEN:
		err = h.sphincter.SendRequest(CMD_OPEN)
	case ACN_CLOSE:
		err = h.sphincter.SendRequest(CMD_CLOSE)
	case ACN_STATE:
		err = h.sphincter.SendRequest(CMD_STATE)
	default:
		fmt.Fprint(w, "INVALID ACTION")
		return
	}

	if err != nil {
		log.Println(err)
		fmt.Fprintf(w, "FAILED TO CALL SPHINCTER")
		return
	}

	closeNotifier := w.(http.CloseNotifier).CloseNotify()

	select {

	case <-closeNotifier:
		// lost connection
		log.Println("lost HTTP connection")
		// keep listening on chan
		go func() {
			<-h.ResponseChan
		}()
		return
	case resp := <-h.ResponseChan:
		// got the response from sphincter
		fmt.Fprintf(w, resp)
	}
}

func main() {

	// init AuthWorker and force a file read
	auth := AuthWorker{HashFile: HASH_FILE}
	if err := auth.ReadHashFile(); err != nil {
		log.Fatal(err)
	}

	// init spincter and listen on serial
	sphincter := Sphincter{
		dev:   "/dev/pts/6",
		speed: 9600,
	}
	serial_chn := make(chan string)
	sphincter.ListenAndReconnect(serial_chn)

	// init and start the web server
	httpResponseChan := make(chan string)
	getHandler := HttpHandler{
		ResponseChan: httpResponseChan,
		auth:         &auth,
		sphincter:    &sphincter,
	}
	go func() { http.ListenAndServe(":8081", getHandler) }()

	// daemon main loop
	for {
		// idle... wait for serial data
		serial_data := <-serial_chn

		if serial_data == RSP_LOCKED ||
			serial_data == RSP_OPEN {
			// select statement for nonblocking write
			select {
			case httpResponseChan <- serial_data:
			default:
			}
		}

		switch serial_data {
		case RSP_LOCKED:
			simpleAPICall("http://api.openlab-augsburg.de/spacecgi.py?update_device_count=0&token=")
		case RSP_UNLOCKED:
			simpleAPICall("http://api.openlab-augsburg.de/spacecgi.py?update_device_count=1&token=")
		case RSP_OPEN:
		case RSP_UNKNOWN:
		}

	}
}

// a simple, non blocking API caller func
func simpleAPICall(url string) {
	go func(url string) {
		resp, err := http.Get(url)
		if err != nil {
			log.Println(err)
		}
		if resp != nil {
			defer resp.Body.Close()
		}
	}(url)
}
