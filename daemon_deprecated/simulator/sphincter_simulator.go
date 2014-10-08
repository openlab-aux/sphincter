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
	if _, err := (*ser).Write([]byte(data + "\r\n")); err != nil {
		log.Fatal(err)
	}
}

func main() {

	speed := flag.Int("speed", 9600, "serial speed (baud)")
	port := flag.String("port", "", "serial port")

	flag.Parse()

	ser, err := serial.OpenPort(&serial.Config{
		Name: *port,
		Baud: *speed})

	if err != nil {
		log.Fatal(err)
	}

	defer ser.Close()

	state := STATE_LOCKED

	buf := make([]byte, 128)

	for {
		n, err := ser.Read(buf)

		if err != nil && err != io.EOF {
			log.Fatal(err)
		}

		sd := strings.TrimSpace(string(buf[:n]))
		log.Println("got serial data: ", sd)

		write(&ser, STATE_BUSY)
		switch sd {

		case "o":
			log.Println("opening the door...")
			if state == STATE_LOCKED {
				time.Sleep(3000 * time.Millisecond)
			} else if state == STATE_UNLOCKED {
				time.Sleep(200 * time.Millisecond)
			}
			write(&ser, STATE_OPEN)
			time.Sleep(200 * time.Millisecond)
			state = STATE_UNLOCKED

		case "c":
			if state == STATE_UNLOCKED {
				log.Println("closing the door...")
				time.Sleep(2800 * time.Millisecond)
			} else if state == STATE_LOCKED {
				log.Println("door allready locked...")
				time.Sleep(time.Millisecond)
				write(&ser, state)
				continue
			}
			state = STATE_LOCKED

		case "r":
			log.Println("REFERENCE RUN...")
			time.Sleep(4000 * time.Millisecond)
			state = STATE_LOCKED

		}
		write(&ser, state)
	}
}
