package main

import (
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/tarm/goserial"
)

type Action uint8

const (
	OPEN  = 0
	CLOSE = 1
	STATE = 2
)

var serialCommands = map[string]string{
	"open":  "open",
	"close": "close",
	"state": "state",
}

type SerialHandler struct {
	port  string
	speed int
}

func (sh *SerialHandler) performAction(action Action) {
}

func (sh *SerialHandler) listen(chn chan string) {

	go func(chn chan string) {
		for {
			for {
				ser, err := serial.OpenPort(&serial.Config{Name: sh.port, Baud: sh.speed})

				if err != nil {
					//log.Fatal(err)
					log.Println(err)
					break
				}
				defer ser.Close()

				var n int
				var out string
				buf := make([]byte, 128)

				// listen for data
				for {
					n, err = ser.Read(buf)

					if err != nil {
						// FIXME handle connection interrupt
						//log.Fatal(err)
						log.Println(err)
						break
					}

					// lines returned from sphincter are terminated with \r\n
					// see http://arduino.cc/en/Serial/Println
					out += string(buf[:n])
					if n > 0 && buf[n-1] == '\n' {
						chn <- strings.Trim(out, "\r\n")
						//sh.onDataReceived(strings.Trim(out, "\r\n"))
						out = ""
					}
				}
			}
			time.Sleep(2 * time.Second)
			log.Println("reconnecting...")
		}
	}(chn)

}

// func (sh *SerialHandler) onDataReceived(data string) {
//
// 	fmt.Println(data)
//
// }

func main() {

	serial_handler := SerialHandler{"/dev/pts/2", 9600}

	mychn := make(chan string)

	serial_handler.listen(mychn)

	// daemon main loop
	for {
		fmt.Println(<-mychn)
	}

}
