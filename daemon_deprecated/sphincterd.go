package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
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
	ACN_RESET = "reset"
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

// ReadHashFile updates the cached hashes if the token hashfile was changed
// since the last access
func (a *AuthWorker) ReadHashFile() error {

	info, err := os.Stat(a.HashFile)
	if err != nil {
		return err
	}

	// check whether file was changed since last read
	if !a.FileLastModified.Equal(info.ModTime()) {

		log.Println("[auth worker] reading hash file " + a.HashFile + " ...")

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

// Auth returns whether a given token matches any hash.
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
			log.Println("[auth worker] user authenticated: " + entry.Mail)
			return true
		}
	}
	log.Println("[auth worker] authentication denied for token: \"" + token + "\"")
	return false
}

type HttpHandler struct {
	sphincter *sphincter.Sphincter
	auth      *AuthWorker
}

func (h HttpHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {

	if err := r.ParseForm(); err != nil {
		log.Println(err)
	}
	action := r.Form.Get("action")
	token := r.Form.Get("token")

	// need auth
	if (action == ACN_OPEN || action == ACN_CLOSE || action == ACN_RESET) && !h.auth.Auth(token) {
		fmt.Fprint(w, "NOT ALLOWED")
		return
	}

	var err error
	var rsp string

	switch action {

	case ACN_OPEN:
		rsp, err = h.sphincter.Open()

	case ACN_CLOSE:
		if s := h.sphincter.State(); s == sphincter.STATE_LOCKED {
			fmt.Fprintf(w, s)
			return
		}
		rsp, err = h.sphincter.Close()

	case ACN_RESET:
		rsp, err = h.sphincter.Reset()

	case ACN_STATE:
		fmt.Fprintf(w, h.sphincter.State())
		return

	default:
		fmt.Fprint(w, "INVALID ACTION")
		return
	}

	if err != nil {
		log.Println(err)
		fmt.Fprintf(w, "FAILED TO CALL SPHINCTER")
		return
	}

	fmt.Fprintf(w, rsp)
}

func main() {

	port := flag.String("port", "", "serial port")
	speed := flag.Int("speed", 9600, "serial speed (baud)")
	hashfile := flag.String("hashfile", "", "file were the token hashes are stored")
	bind := flag.String("bind", ":8080", "[host]:port to bind the HTTP server to")

	flag.Parse()

	if *hashfile == "" {
		log.Fatal(errors.New("no hash file given."))
	}

	if *port == "" {
		log.Fatal(errors.New("no serial port given."))
	}

	// init AuthWorker and force a file read
	auth := AuthWorker{HashFile: *hashfile}
	if err := auth.ReadHashFile(); err != nil {
		log.Fatal(err)
	}

	// init spincter and listen on serial
	sph := sphincter.New(*port, *speed)
	sphincterResponses := make(chan string)
	sph.ListenAndReconnect(sphincterResponses)

	// init and start the web server
	getHandler := HttpHandler{
		auth:      &auth,
		sphincter: sph,
	}
	go func() { log.Fatal(http.ListenAndServe(*bind, getHandler)) }()

	// daemon main loop
	for {
		// idle... wait for serial data
		data := <-sphincterResponses

		switch data {
		case sphincter.STATE_OPEN:
		case sphincter.STATE_LOCKED:
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
