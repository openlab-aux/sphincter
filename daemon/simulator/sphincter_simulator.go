package main

import (
	"flag"
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

func write(ser *io.ReadWriteCloser, data string) {
	if _, err := ser.Write([]byte(data)); err != nil {
		log.Fatal(err)
	}
}

func main() {

	speed := flag.Int("speed", 9600, "serial speed (baud)")
	port := flag.String("port", "", "serial port")

	flag.Parse()

	ser, err := serial.OpenPort(&serial.Config{
		Name: *dev,
		Baud: *speed})

	if err != nil {
		log.Fatal(err)
	}

	defer ser.Close()

	state := STATE_LOCKED

	buf := make([]byte, 128)
	var inp string
	var out string

	for {
		n, err := ser.Read(buf)

		if err != nil && err != io.EOF {
			log.Fatal(err)
		}

		inp += string(buf[:n])
		if n > 0 && buf[n-1] == '\n' {
			sd := strings.Trim(inp, "\r\n")

			write(&ser, STATE_BUSY)
			switch sd {

			case "o":
				if state == STATE_LOCKED {
					time.Sleep(3 * time.Second)
				} else if state == STATE_UNLOCKED {
					time.Sleep(0.2 * time.Second)
				}
				write(&ser, STATE_OPEN)
				time.Sleep(0.2 * time.Second)
				write(&ser, STATE_UNLOCKED)
				state = STATE_UNLOCKED

			case "c":
				if state == STATE_UNLOCKED {
					time.Sleep(2.8 * time.Second)
				} else if state == STATE_LOCKED {
					continue
				}
				write(&ser, STATE_LOCKED)
				state = STATE_LOCKED

			case "s":

			default:
				write(&ser, STATE_UNKNOWN)

			}

			inp = ""
		}

	}
}
