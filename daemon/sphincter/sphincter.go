package sphincter

import (
	"errors"
	"io"
	"log"
	"strings"
	"time"

	"github.com/tarm/goserial"
)

const (
	STATE_OPEN     string = "OPEN"
	STATE_UNLOCKED string = "UNLOCKED"
	STATE_LOCKED   string = "LOCKED"
	STATE_UNKNOWN  string = "UNKNOWN"
	STATE_BUSY     string = "BUSY"
)

// New returns the pointer to a newly initialized Sphincter.
func New(device string, speed int) *Sphincter {
	s := Sphincter{
		dev:   device,
		speed: speed,
	}
	return &s
}

// Sphincter handles the sphincter device connected via RS-232.
type Sphincter struct {
	dev      string
	speed    int
	state    string
	listener chan string

	io.ReadWriteCloser
}

// connect establishes the serial connection to sphincter.
func (s *Sphincter) connect() bool {

	var err error

	s.ReadWriteCloser, err = serial.OpenPort(&serial.Config{
		Name: s.dev,
		Baud: s.speed})

	if err != nil {
		log.Println(err)
		return false
	}
	log.Println("[sphincter] connected to sphincter on port " + s.dev)
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

					if err != nil && err != io.EOF {
						log.Println(err)
						break
					}

					// Read until line end
					// lines returned from sphincter are terminated with "\r\n"
					// see http://arduino.cc/en/Serial/Println
					out += string(buf[:n])
					if n > 0 && buf[n-1] == '\n' {
						sd := strings.Trim(out, "\r\n")
						log.Println("[sphincter] got serial data: \"" + sd + "\"")

						s.state = sd // update state cache
						if sd != STATE_BUSY {
							chn <- sd // pass serial data to initiator

							// if we have a listener, pass the response to it
							// and close/remove it.
							if s.listener != nil && sd != STATE_UNLOCKED {
								select {
								case s.listener <- sd:
									log.Printf("[sphincter] sent response '%s' to listener", sd)
								default:
									log.Println("[sphincter] response listener seems dead")
								}

								close(s.listener)
								s.listener = nil
							}
						}
						out = ""
					}
				}
			}
			time.Sleep(5 * time.Second)
			log.Println("[sphincter] reconnecting...")
		}
	}(chn)
}

func (s *Sphincter) request(rq string) (string, error) {
	if s.state != STATE_BUSY {
		log.Println("[sphincter] sending serial request: \"" + rq + "\"")
		if s.ReadWriteCloser == nil {
			return "", errors.New("write " + s.dev + ": no serial connection established")
		}
		_, err := s.Write([]byte(rq))
		if err != nil {
			return "", err
		}

		chn := make(chan string)
		s.listener = chn
		return <-chn, nil
	}
	return STATE_BUSY, nil
}

// Open sends an open request to sphincter and blocks until serial data
// returns.
func (s *Sphincter) Open() (string, error) {
	return s.request("o")
}

// Close sends a close request to sphincter and blocks until serial data
// returns.
func (s *Sphincter) Close() (string, error) {
	return s.request("c")
}

// State returns the current (cached) state of sphincter.
func (s *Sphincter) State() string {
	if s.state != "" {
		return s.state
	}
	return STATE_UNKNOWN
}
