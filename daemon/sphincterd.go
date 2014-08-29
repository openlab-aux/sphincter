package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/tarm/goserial"
)

const (
	// Commands for sphincter
	CMD_OPEN  string = "o"
	CMD_CLOSE string = "c"
	CMD_STATE string = "s"
	CMD_REF   string = "r"

	// Response codes from sphincter
	RSP_OPEN     string = "OPEN"
	RSP_UNLOCKED string = "UNLOCKED"
	RSP_LOCKED   string = "LOCKED"
	RSP_UNKNOWN  string = "UNKNOWN"
	RSP_ACK      string = "ACK"

	// Http GET actions
	ACN_OPEN  string = "open"
	ACN_CLOSE string = "close"
	ACN_STATE string = "state"

	HASH_FILE string = "./hashes.json"
)

// Struct that handles sphincter connected via RS-232
type Sphincter struct {
	dev   string
	speed int

	io.ReadWriteCloser
}

// connect to sphincter
func (s *Sphincter) connect() bool {

	var err error

	s.ReadWriteCloser, err = serial.OpenPort(&serial.Config{
		Name: s.dev,
		Baud: s.speed})

	if err != nil {
		log.Println(err)
		return false
	}
	log.Println("connected to sphincter on port " + s.dev)
	return true
}

// ListenAndReconnect listens for serial data and infinitly tries to reconnect
func (s *Sphincter) ListenAndReconnect(chn chan string) {

	go func(chn chan string) {

		var err error
		var n int
		var out string
		buf := make([]byte, 128)

		// loop for reconnecting
		for {
			// close port to be able to reopen it
			if s.ReadWriteCloser != nil {
				s.Close()
			}

			if s.connect() {
				defer s.Close()

				// listen for data
				for {
					n, err = s.Read(buf)

					if err != nil {
						log.Println(err)
						break
					}

					// Read until line end
					// lines returned from sphincter are terminated with "\r\n"
					// see http://arduino.cc/en/Serial/Println
					out += string(buf[:n])
					if n > 0 && buf[n-1] == '\n' {
						sd := strings.Trim(out, "\r\n")
						out = ""
						chn <- sd
						log.Println("got serial data: \"" + sd + "\"")
					}
				}
			}
			time.Sleep(5 * time.Second)
			log.Println("reconnecting...")
		}
	}(chn)
}

// Send requests to sphincter
func (s *Sphincter) SendRequest(rq string) error {
	log.Println("sending serial data: \"" + rq + "\"")
	if s.ReadWriteCloser == nil {
		return errors.New("write " + s.dev + ": no serial connection established")
	}
	_, err := s.Write([]byte(rq))
	if err != nil {
		return err
	}
	return nil
}

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
