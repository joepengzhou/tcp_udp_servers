#!/usr/bin/env bash
# multi_nc_send.sh
# 并发模拟多个客户端，每个用不同 source port (-p)
# Usage:
#   ./multi_nc_send.sh SERVER PORT CLIENTS MSGS_PER_CLIENT DELAY
# Example:
#   ./multi_nc_send.sh 127.0.0.1 5001 10 200 01

SERVER=${1:-127.0.0.1}
PORT=${2:-5001}
CLIENTS=${3:-10}
MSGS=${4:-100}
DELAY=${5:-0.01}   # seconds between messages per client (can be 0)

SRC_BASE=55000     # starting source port for first virtual client
PIDS=()

# check for nc
if ! command -v nc >/dev/null 2>&1; then
  echo "ERROR: nc not found. Install with: sudo apt install netcat-openbsd"
  exit 1
fi

for ((i=0;i<CLIENTS;i++)); do
  SRC_PORT=$((SRC_BASE + i))
  (
    for ((m=1;m<=MSGS;m++)); do
      MSG="client$(printf "%02d" $i)_msg$(printf "%04d" $m)"
      # Send using netcat; -u = UDP, -p = source port, -w1 = wait 1s (close)
      # Some nc implementations require sudo to bind low ports; for high ports it's ok.
      #printf "%s" "$MSG" | nc -u -p $SRC_PORT -w1 $SERVER $PORT
      printf "%s" "$MSG" | nc -p $SRC_PORT -w1 $SERVER $PORT
      # optional logging
      echo "sent from src_port=$SRC_PORT -> $SERVER:$PORT : $MSG"
      if (( $(echo "$DELAY > 0" | bc -l) )); then
        sleep $DELAY
      fi
    done
  ) &
  PIDS+=($!)
done

echo "Launched $CLIENTS clients (pids: ${PIDS[*]})."
echo "To stop: kill pids or press Ctrl+C and then run: kill ${PIDS[*]}"
wait
