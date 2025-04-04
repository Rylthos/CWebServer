#!/bin/sh

if ! iptables -C OUTPUT -p tcp --tcp-flags RST RST -j DROP; then
    iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP
fi
./web "$@"
