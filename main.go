package main

import (
	"fmt"
  "os"
	"os/exec"
)

func toMsg(json []byte) []byte {

  l := len(json)

  header := []byte(fmt.Sprintf("Content-Length: %d\r\n\r\n", l))
  msg := append(header, json...)
  return msg
}
func main() {

	cmd := exec.Command("harper-ls")
	stdout, _ := cmd.StdoutPipe()
	cmd.Stderr = cmd.Stdout

stdin, err := cmd.StdinPipe()
if err != nil {
    fmt.Println(err)
}


	cmd.Start()

  init, _ := os.ReadFile("/home/jens/git/glib-reader/test/json/init.json")

  stdin.Write(toMsg(init))
  open, _ := os.ReadFile("/home/jens/git/glib-reader/test/json/didOpen.json")
  stdin.Write(toMsg(open))
  dia, _ := os.ReadFile("/home/jens/git/glib-reader/test/json/diagnostic.json")
  stdin.Write(toMsg(dia))

	for {
		tmp := make([]byte, 1024)
		_, err := stdout.Read(tmp)
		fmt.Print(string(tmp))
		if err != nil {
			break
		}
	}
}
