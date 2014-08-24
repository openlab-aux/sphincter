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

	HASH_FILE string = "./token.json"
)

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
			if s.connect() {
				defer s.Close()

				// FIXME: flush buffer

				// listen for data
				for {
					n, err = s.Read(buf)

					if err != nil {
						log.Println(err)
						break
					}

					// lines returned from sphincter are terminated with "\r\n"
					// see http://arduino.cc/en/Serial/Println
					out += string(buf[:n])
					if n > 0 && buf[n-1] == '\n' {
						chn <- strings.Trim(out, "\r\n")
						out = ""
					}
				}
			}
			time.Sleep(5 * time.Second)
			log.Println("reconnecting...")
		}
	}(chn)
}

func (s *Sphincter) PerformAction(action string) {
	_, err := s.Write([]byte(action))
	if err != nil {
		// FIXME better error handling, not just call log.Fatal :/
		log.Fatal(err)
	}
}

type HashTable []struct {
	Mail string
	Hash string
	Salt string
}

func (ht *HashTable) ReadHashFile(filename string) error {
	content, err := ioutil.ReadFile(filename)
	if err != nil {
		return err
	}

	if err = json.Unmarshal(content, ht); err != nil {
		return err
	}

	return nil
}

func (ht *HashTable) Auth(token string) bool {
	for _, entry := range *ht {
		// compute salted hash from token
		h256 := sha256.New()
		io.WriteString(h256, token)
		io.WriteString(h256, entry.Salt)
		chash := hex.EncodeToString(h256.Sum(nil))

		// check, if computed hash matches hash from table
		if chash == entry.Hash {
			return true
		}
	}
	return false
}

func main() {

	var ht HashTable
	if err := ht.ReadHashFile(HASH_FILE); err != nil {
		log.Fatal(err)
	}

	sphincter := Sphincter{
		"/dev/pts/4",
		9600,
		nil}

	var httpRespQueue []chan string
	serial_chn := make(chan string)

	sphincter.ListenAndReconnect(serial_chn)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {

		// FIXME set timeout for http request

		if err := r.ParseForm(); err != nil {
			// TODO err handling
		}

		switch r.Form.Get("action") {

		case ACN_OPEN, ACN_CLOSE:
			if !ht.Auth(r.Form.Get("token")) {
				fmt.Fprint(w, "NOT ALLOWED")
				return
			}

		case ACN_STATE:

		default:
			fmt.Fprint(w, "INVALID ACTION")
			return
		}

		// TODO call sphincter
		chn := make(chan string)
		httpRespQueue = append(httpRespQueue, chn)

		// wait for corresponding serial response
		fmt.Fprint(w, <-chn)

	})
	go func() { http.ListenAndServe(":8080", nil) }()

	// daemon main loop
	for {
		// wait for serial data
		serial_data := <-serial_chn

		// check if there are waiting http connections
		if len(httpRespQueue) > 0 {
			httpRespQueue[0] <- serial_data
			httpRespQueue = httpRespQueue[1:]
		}
	}

}
