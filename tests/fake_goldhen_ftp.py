#!/usr/bin/env python3
"""An FTP server that behaves as GoldHEN's v2.2 was measured to, for the USB tests.

  fake_goldhen_ftp.py <port> <folder served as /mnt/usb0> <command log>

Measured on the console, and kept here on purpose: ABOR and EPSV answer 502, a
transfer whose data connection is closed early answers 426, and REST followed by
STOR does not write at the offset: the file is emptied first, so a client that
resumes that way loses what it had. A file ".quota" in the folder holds the bytes
a file may reach before the "drive" is full.
"""
import os
import socket
import sys
import threading

PORT, ROOT, LOG = int(sys.argv[1]), sys.argv[2], sys.argv[3]
log_lock = threading.Lock()


def note(text):
    with log_lock, open(LOG, "a") as out:
        out.write(text + "\n")


def real(path):
    return ROOT + path[len("/mnt/usb0"):] if path.startswith("/mnt/usb0") else None


class Session(threading.Thread):
    def __init__(self, control):
        super().__init__(daemon=True)
        self.control, self.listener, self.rest = control, None, 0
        self.cwd, self.rename_from = "/", None

    def say(self, line):
        self.control.sendall((line + "\r\n").encode())

    def path(self, name):
        return name if name.startswith("/") else self.cwd.rstrip("/") + "/" + name

    def accept(self):
        listener, self.listener = self.listener, None
        data, _ = listener.accept()
        listener.close()
        return data

    def run(self):
        self.say("220 GoldHEN FTP server v2.2")
        buffered = b""
        while True:
            while b"\n" not in buffered:
                try:
                    got = self.control.recv(4096)
                except OSError:
                    return
                if not got:
                    return
                buffered += got
            raw, buffered = buffered.split(b"\n", 1)
            line = raw.decode(errors="replace").rstrip("\r")
            verb, _, arg = line.partition(" ")
            note(line)
            if verb == "QUIT":
                self.say("221 Service closing control connection.")
                return
            self.handle(verb.upper(), arg)

    def handle(self, verb, arg):
        if verb == "USER":
            self.say("230 User logged in, proceed.")
        elif verb == "PASS":
            self.say("202 Command not implemented, superfluous at this site.")
        elif verb in ("TYPE", "NOOP"):
            self.say("200 Command okay.")
        elif verb == "PWD":
            self.say('257 "%s" is the current directory.' % self.cwd)
        elif verb == "CWD":
            target = self.path(arg)
            if target in ("/", "/mnt") or (real(target) and os.path.isdir(real(target))):
                self.cwd = target
                self.say("250 Requested file action okay, completed.")
            else:
                self.say("550 Requested action not taken. File unavailable.")
        elif verb == "PASV":
            self.listener = socket.socket()
            self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.listener.bind(("127.0.0.1", 0))
            self.listener.listen(1)
            port = self.listener.getsockname()[1]
            self.say("227 Entering Passive Mode (127,0,0,1,%d,%d)." % (port // 256, port % 256))
        elif verb == "REST":
            self.rest = int(arg)
            self.say("350 Resuming at %d." % self.rest)
        elif verb == "SIZE":
            target = real(self.path(arg))
            if target and os.path.isfile(target):
                self.say("213 %d" % os.path.getsize(target))
            else:
                self.say("550 Requested action not taken. File unavailable.")
        elif verb == "LIST":
            self.listing()
        elif verb == "RETR":
            self.retrieve(arg)
        elif verb in ("STOR", "APPE"):
            self.store(verb, arg)
        elif verb == "MKD":
            target = real(self.path(arg))
            try:
                os.mkdir(target)
                self.say('257 "%s" created.' % arg)
            except (OSError, TypeError):
                self.say("550 Requested action not taken. File unavailable.")
        elif verb in ("DELE", "RMD"):
            target = real(self.path(arg))
            try:
                (os.remove if verb == "DELE" else os.rmdir)(target)
                self.say("250 Requested file action okay, completed.")
            except (OSError, TypeError):
                self.say("550 Requested action not taken. File unavailable.")
        elif verb == "RNFR":
            self.rename_from = real(self.path(arg))
            self.say("350 Requested file action pending further information.")
        elif verb == "RNTO":
            try:
                os.rename(self.rename_from, real(self.path(arg)))
                self.say("250 Requested file action okay, completed.")
            except (OSError, TypeError):
                self.say("550 Requested action not taken. File unavailable.")
        else:  # ABOR and EPSV among them
            self.say("502 Command not implemented.")

    def listing(self):
        self.say("150 File status okay; about to open data connection.")
        data = self.accept()
        folder = real(self.cwd)
        rows = ""
        for name in sorted(os.listdir(folder)) if folder else []:
            if name == ".quota":
                continue
            full = os.path.join(folder, name)
            kind = "d" if os.path.isdir(full) else "-"
            rows += "%srwxrwxrwx 1 -1 -1 %d Nov  9 2026 %s\r\n" % (kind, os.path.getsize(full), name)
        data.sendall(rows.encode())
        data.close()
        self.say("226 Closing data connection. Requested file action successful.")

    def retrieve(self, name):
        offset, self.rest = self.rest, 0
        target = real(self.path(name))
        if not target or not os.path.isfile(target):
            if self.listener:
                self.listener.close()
                self.listener = None
            self.say("550 Requested action not taken. File unavailable.")
            return
        self.say("150 File status okay; about to open data connection.")
        data = self.accept()
        try:
            with open(target, "rb") as source:
                source.seek(offset)
                while True:
                    chunk = source.read(65536)
                    if not chunk:
                        break
                    data.sendall(chunk)
            data.close()
            self.say("226 Closing data connection. Requested file action successful.")
        except OSError:
            data.close()
            self.say("426 Connection closed; transfer aborted.")

    def store(self, verb, name):
        offset, self.rest = self.rest, 0
        target = real(self.path(name))
        if not target or not os.path.isdir(os.path.dirname(target)):
            if self.listener:
                self.listener.close()
                self.listener = None
            self.say("550 Requested action not taken. File unavailable.")
            return
        if offset:
            note("TRUNCATED BY REST+STOR " + name)
        quota_file = os.path.join(ROOT, ".quota")
        room = int(open(quota_file).read()) if os.path.exists(quota_file) else None
        self.say("150 File status okay; about to open data connection.")
        data = self.accept()
        with open(target, "ab" if verb == "APPE" else "wb") as out:
            if room is not None:
                room = max(0, room - out.tell())
            while True:
                chunk = data.recv(65536)
                if not chunk:
                    break
                if room is not None:
                    if len(chunk) > room:
                        out.write(chunk[:room])
                        data.close()
                        self.say("451 Requested action aborted. Local error in processing.")
                        return
                    room -= len(chunk)
                out.write(chunk)
        data.close()
        self.say("226 Closing data connection. Requested file action successful.")


def main():
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", PORT))
    server.listen(16)
    while True:
        control, _ = server.accept()
        Session(control).start()


main()
