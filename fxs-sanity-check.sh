#!/bin/bash -e

asterisk -x 'module load fxstest.so' >&/dev/null || :

refresh_fxs() {
IFS='
' eval $1='( $(asterisk -x 'fxstest' 2>/dev/null || :) )'
}

is_stuck() {
  [ "$(echo "$1" |
       sed 's/,/\n/g' |
       sed 's/^[^:]*: *//g;t;d' |
       sort -u |
       wc -l)" = 1 ]
}

refresh_fxs dat
for i in 1 2 3; do
  if is_stuck "${dat[$i]}"; then
    asterisk -x 'core show calls' 2>/dev/null |
     egrep '^0 active calls' >&/dev/null || exit 0

    { echo "Detected stuck FXS port"
      echo
      asterisk -x "core show channels" 2>&1
      for msg in "${dat[@]}"; do echo "$msg"; done

      echo "Restarting Asterisk and reloading DAHDI drivers."
      { /etc/init.d/asterisk stop
        /etc/init.d/dahdi stop
        /etc/init.d/dahdi start
        /etc/init.d/asterisk start
      } >/dev/null 2>&1

      refresh_fxs dat
      if is_stuck "${dat[1]}" || is_stuck "${dat[2]}" || is_stuck "${dat[3]}"; then
        echo "Problem did not get resolved. Something is really wrong!"
        for msg in "${dat[@]}"; do echo "$msg"; done
      else
        echo "The problem appears to be resolved now"
      fi
    }  ### | Mail -s 'Asterisk DAHDI driver problem detected' -r root root

    exit 0
  fi
done
