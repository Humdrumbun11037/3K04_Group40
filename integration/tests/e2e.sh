#!/usr/bin/env bash
# End-to-end test: the real integrated programs (hms_live + adc_hms_link)
# driven on a timeline, with the HMS state checked through its dashboard API.
#   Normal:     dispense, Ping (succeeds: still dispensing), dispense again -> HMS
#               counts, InventoryLow; 3 bad logins -> UnauthorizedAccess; a wrong-key
#               packet -> IncorrectMessageFormat; Components shows ADC-3 as REAL.
#   Off-normal: link cut -> the ADC dispenses, Ping fails -> OFFLINE, and it still
#               dispenses; the HMS marks ADC-3 Offline (AdcOffline). Restored -> queued
#               messages arrive (noted as late, with the ADC's time), Ping succeeds,
#               ADC-3 Online, AdcOffline stays open.
#   Swap ADC:   the selection screen at start (ADC-3); "4" at the login menu swaps to
#               ADC-1 (its stock loaded from the HMS; a restock from the menu and a dispense
#               counted there) and back.
#   Hardware:   drawer stuck open -> after Subgroup A's 30 s timeout the ADC locks
#               (Fault mode) and the HMS gets TIMEOUT_FAULT.
# Run from integration/ after `mingw32-make` (the e2e target does both). About 75 s.
# Needs curl and python. Uses ports 8081 and 9040, so no other HMS may be running.
set -u
cd "$(dirname "$0")/.."
OUT=build/e2e
mkdir -p "$OUT" run
EXE=""; [ -f build/hms_live.exe ] && EXE=".exe"
API=http://127.0.0.1:8081/api
FAILED=0

check() {   # check "<description>" "<python expression over s (the state)>"
    if curl -s "$API/state" | python -c "import json,sys; s=json.load(sys.stdin); sys.exit(0 if ($2) else 1)"; then
        echo "  ok    $1"
    else
        echo "  FAIL  $1"; FAILED=1
    fi
}
expect() {  # expect "<description>" <file> "<text>"
    if grep -qF "$3" "$2"; then echo "  ok    $1"; else echo "  FAIL  $1"; FAILED=1; fi
}
count() { echo "[i['count'] for i in s['stock'] if i['adc']=='ADC-3' and i['medication']=='$1'][0]"; }
count_at() { echo "[i['count'] for i in s['stock'] if i['adc']=='$1' and i['medication']=='$2'][0]"; }
status() { echo "[a['status'] for a in s['adcs'] if a['name']=='ADC-3'][0]"; }
alarm() { echo "[a for a in s['alarms'] if a['adc']=='$2' and a['type']=='$1']"; }
feed() { echo "any('$1' in f['text'] for f in s['feed'])"; }
stop_all() {
    taskkill //F //IM hms_live.exe > /dev/null 2>&1 || pkill -f build/hms_live > /dev/null 2>&1
    taskkill //F //IM adc_hms_link.exe > /dev/null 2>&1 || pkill -f build/adc_hms_link > /dev/null 2>&1
    rm -f run/drawer_mode.txt
}
trap stop_all EXIT
rm -f run/drawer_mode.txt "$OUT/swap_checked"

# The HMS, with its console commands on a timeline: cut at 12 s, restore at 24 s.
( sleep 12; echo cut; sleep 12; echo restore; sleep 90 ) | ./build/hms_live$EXE > "$OUT/hms.out" 2>&1 &
sleep 2
curl -s -X POST "$API/login?user=admin_alice&password=hash000" > /dev/null

# The ADC, typed on a timeline. Subgroup A's menus: logged out 1 login / 3 shut down;
# logged in 1 dispense / 5 ping / 8 clear faults / 9 log out / 10 shut down.
(
  printf '3\n'                                                            # ~2 s: the selection screen: ADC-3
  printf '1\nnurse_jane\nhash123\n1\nTylenol\n60\n5\n1\nTylenol\n1\n9\n'   # dispense, Ping OK, dispense
  printf '1\nnurse_jane\nbad\n1\nnurse_jane\nbad\n1\nnurse_jane\nbad\n'    # lockout
  sleep 4
  printf '1\ntech_tom\nhash789\n8\n9\n'                                   # technician clears it
  sleep 9                                                                 # (link cut at 12 s)
  printf '1\nnurse_jane\nhash123\n1\nMorphine\n5\n5\n1\nMorphine\n2\n'    # ~15 s: dispense, Ping fails, dispense
  sleep 13                                                                # (link restored at 24 s)
  printf '5\n'                                                            # ~28 s: Ping succeeds
  sleep 2
  printf '9\n4\n1\n1\npharm_bob\nhash456\n2\nMorphine\n20\n1\nMorphine\n5\n'   # ~30 s: Swap to ADC-1, restock 20, dispense 5
  # Wait until the checks below have looked at ADC-1 (they lag behind this timeline).
  for _ in $(seq 150); do [ -f "$OUT/swap_checked" ] && break; sleep 0.2; done
  printf '9\n4\n3\n1\nnurse_jane\nhash123\n'                              # Swap back to ADC-3
  sleep 1
  echo stuck > run/drawer_mode.txt
  printf '1\nTylenol\n5\n'                                                # the drawer sticks open
  sleep 36
  printf '10\n'                                                           # shut down
) | ./build/adc_hms_link$EXE > "$OUT/adc.out" 2> "$OUT/adc.err" &

echo "Normal path"
sleep 6
expect "selection screen shown, ADC-3 picked, its stock loaded from the HMS" "$OUT/adc.out" \
       "This screen is now ADC-3 (Ward 5). Stock loaded from the HMS: Morphine 50, Tylenol 100."
expect "Subgroup A's menu shows the cabinet and the Swap ADC option" "$OUT/adc.out" " [4] Swap ADC (demo only)"
check "dispense, Ping, dispense: Tylenol at ADC-3 is 100 - 60 - 1 = 39" "$(count Tylenol) == 39"
expect "ADC still dispensing after a successful Ping (Ping fix)" "$OUT/adc.out" "Remaining Tylenol: 39"
check "InventoryLow (High) for ADC-3 Tylenol" "any(a['severity']=='High' and a['detail']=='Tylenol' for a in $(alarm InventoryLow ADC-3))"
check "lockout: UnauthorizedAccess (Critical) for ADC-3" "any(a['severity']=='Critical' for a in $(alarm UnauthorizedAccess ADC-3))"
check "ADC-3 Online" "$(status) == 'Online'"
[ "$(curl -s -o /dev/null -w '%{http_code}' -X POST "$API/advance?seconds=180")" = 409 ] \
    && echo "  ok    Advance refused while the link is live" || { echo "  FAIL  Advance was not refused"; FAILED=1; }
./build/adc_stub$EXE --wrong-key "3|Heartbeat" > /dev/null
check "wrong-key packet: IncorrectMessageFormat (Critical)" "any(a['severity']=='Critical' for a in $(alarm IncorrectMessageFormat Hospital))"
check "Components: ADC-3 REAL, the other ADCs simulated" \
      "any(c['real'] and c['name']=='Real cabinets: ADC-3' for c in s['components']) and any((not c['real']) and c['name']=='Other ADCs and their link' for c in s['components'])"

echo "Off-normal path (link cut)"
sleep 13   # ~21 s: cut at 12 s, ADC dispensed, pinged and dispensed again at ~15 s
expect "ADC dispensed Morphine while the link was cut" "$OUT/adc.out" "Remaining Morphine: 45"
expect "failed Ping put the ADC OFFLINE" "$OUT/adc.out" "OFFLINE (Local Dispensing Only)"
expect "ADC still dispensed while OFFLINE (Ping fix, BH3)" "$OUT/adc.out" "Remaining Morphine: 43"
expect "ADC reported the link down" "$OUT/adc.err" "HMS not reachable"
check "HMS has not seen the Morphine withdrawals yet (50)" "$(count Morphine) == 50"
curl -s -X POST "$API/advance?seconds=180" > /dev/null   # allowed now: the link is cut
check "ADC-3 Offline after 3 missed periods" "$(status) == 'Offline'"
check "AdcOffline (Critical) for ADC-3" "any(a['severity']=='Critical' for a in $(alarm AdcOffline ADC-3))"
check "simulated ADC-1 unaffected" "[a['status'] for a in s['adcs'] if a['name']=='ADC-1'][0] == 'Online'"

echo "Link restored"
sleep 8    # ~29 s: restored at 24 s; the ADC retries every 2 s and pinged at ~28 s
check "queued withdrawals arrived: Morphine 43" "$(count Morphine) == 43"
check "queued CONNECTION_LOSS arrived (Low)" "any(a['severity']=='Low' for a in $(alarm ConnectionLoss ADC-3))"
check "late messages noted with their ADC time" "$(feed 'it waited while the link was down')"
check "ADC-3 back Online" "$(status) == 'Online'"
check "AdcOffline stays open until resolved" "all(a['state']!='Resolved' for a in $(alarm AdcOffline ADC-3))"
expect "ADC reported the link back" "$OUT/adc.err" "Connected to the HMS"

echo "Swap ADC (demo cabinet selection)"
sleep 3    # ~32 s: swapped to ADC-1 at ~30 s and dispensed 5 Morphine there
check "ADC-1 is now driven by the cabinet program, ADC-3 handed back" \
      "any(c['name']=='Real cabinets: ADC-1' for c in s['components'])"
check "restock and dispense went to ADC-1: Morphine 40 + 20 - 5 = 55" "$(count_at ADC-1 Morphine) == 55"
check "ADC-3's count unchanged: Morphine 43" "$(count Morphine) == 43"
expect "swapped to ADC-1 with ADC-1's stock from the HMS" "$OUT/adc.out" \
       "This screen is now ADC-1 (Ward 3 West). Stock loaded from the HMS: Insulin 30, Morphine 40."
touch "$OUT/swap_checked"   # the ADC timeline swaps back now
sleep 2
expect "swapped back: ADC-3's counts reloaded from the HMS" "$OUT/adc.out" \
       "This screen is now ADC-3 (Ward 5). Stock loaded from the HMS: Morphine 43, Tylenol 39."
check "ADC-1 back with the simulator" "[a['silenced'] for a in s['adcs'] if a['name']=='ADC-1'][0] == False"

echo "Hardware fault (drawer stuck open)"
sleep 31   # the drawer stuck about 1 s after the swap back; Subgroup A's timeout is 30 s
expect "ADC locked itself: drawer timeout" "$OUT/adc.out" "SYSTEM LOCKED: DRAWER TIMEOUT"
check "TIMEOUT_FAULT reached the HMS (Critical, delivered, not resolvable)" "$(feed 'ADC alarm code TIMEOUT_FAULT')"
check "the stuck dispense was counted: Tylenol 34" "$(count Tylenol) == 34"

[ $FAILED = 0 ] && echo "e2e: all checks passed" || echo "e2e: FAILED (logs in $OUT/)"
exit $FAILED
