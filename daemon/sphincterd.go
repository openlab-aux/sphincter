package main

import (
	"fmt"
	"io"
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

// listenAndReconnect listens for serial data and infinitly tries to reconnect
func (s *Sphincter) listenAndReconnect(chn chan string) {

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

func (s *Sphincter) performAction(action string) {

	_, err := s.Write([]byte(action))
	if err != nil {
		log.Fatal(err)
	}

}

func main() {

	sphincter := Sphincter{
		"/dev/pts/4",
		9600,
		nil}

	var httpRespQueue []*chan string
	serial_chn := make(chan string)

	sphincter.listenAndReconnect(serial_chn)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {

		// TODO check auth

		if err := r.ParseForm(); err != nil {
		}

		switch r.Form.Get("action") {
		case ACN_CLOSE, ACN_OPEN, ACN_STATE:
			chn := make(chan string)
			httpRespQueue = append(httpRespQueue, &chn)

			// wait for corresponding serial response
			fmt.Fprint(w, <-chn)
		default:
			fmt.Fprint(w, "INVALID ACTION")
			return
		}

	})
	go func() { http.ListenAndServe(":8080", nil) }()

	// daemon main loop
	for {
		// wait for serial data
		serial_data := <-serial_chn

		// check if there are waiting http connections
		if len(httpRespQueue) > 0 {
			*httpRespQueue[0] <- serial_data
			httpRespQueue = httpRespQueue[1:]
		}
	}
}
