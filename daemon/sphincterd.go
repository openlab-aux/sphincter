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
	ACTION_OPEN  string = "o"
	ACTION_CLOSE string = "c"
	ACTION_STATE string = "s"
	ACTION_REF   string = "r"
)

const (
	RESPONSE_OPEN     string = "OPEN"
	RESPONSE_UNLOCKED string = "UNLOCKED"
	RESPONSE_LOCKED   string = "LOCKED"
	RESPONSE_UNKNOWN  string = "UNKNOWN"
	RESPONSE_ACK      string = "ACK"
)

type SerialHandler struct {
	port       string
	speed      int
	serial_con io.ReadWriteCloser
}

func (sh *SerialHandler) connect() bool {

	// FIXME: flush buffer

	var err error

	sh.serial_con, err = serial.OpenPort(&serial.Config{
		Name: sh.port,
		Baud: sh.speed})

	if err != nil {
		log.Println(err)
		return false
	}
	return true

}

func (sh *SerialHandler) listen(chn chan string) {

	go func(chn chan string) {

		var err error
		var n int
		var out string
		buf := make([]byte, 128)

		// loop for reconnecting
		for {
			if sh.connect() {

				defer sh.serial_con.Close()

				// listen for data
				for {
					n, err = sh.serial_con.Read(buf)

					if err != nil {
						log.Println(err)
						break
					}

					// lines returned from sphincter are terminated with "\r\n"
					// see http://arduino.cc/en/Serial/Println
					out += string(buf[:n])
					if n > 0 && buf[n-1] == '\n' {
						out = strings.Trim(out, "\r\n")

						sh.onDataReceived(out)
						chn <- out

						out = ""
					}
				}
			}
			time.Sleep(5 * time.Second)
			log.Println("reconnecting...")
		}
	}(chn)
}

// onDataReceived can be used to trigger actions on a state change
func (sh *SerialHandler) onDataReceived(data string) {

	// FIXME: add beehive/spaceapi call
	switch data {
	case RESPONSE_ACK:
	case RESPONSE_LOCKED:
	case RESPONSE_UNLOCKED:
	case RESPONSE_OPEN:
	case RESPONSE_UNKNOWN:
	default:
	}
}

func (sh *SerialHandler) performAction(action string) {

	_, err := sh.serial_con.Write([]byte(action))
	if err != nil {
		log.Fatal(err)
	}

}

func main() {

	serial_handler := SerialHandler{"/dev/pts/2", 9600, nil}
	serial_chn := make(chan string)

	// start serial main loop
	serial_handler.listen(serial_chn)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {

		err := r.ParseForm()
		if err != nil {
		}

		var action string

		switch r.Form.Get("action") {
		case "open":
			action = ACTION_OPEN
		case "close":
			action = ACTION_CLOSE
		case "state":
			action = ACTION_STATE
		default:
		}

		serial_handler.performAction(action)

		fmt.Fprint(w, <-serial_chn)
	})

	func() { log.Fatal(http.ListenAndServe(":8080", nil)) }()

}
