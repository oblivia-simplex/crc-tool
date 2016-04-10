#! /usr/bin/env bash
###############################
# A testing tool for CRC.c
#
# Author: Olivia Lucca Fraser
# ID: B00109376
# Date: October 25, 2015
###############################

OUTFILE=crc-experiment.out
(figlet "CRC Tester" 2> /dev/null || echo -e "CRC TESTER\n=-=-=-=-=-\n") 

gcc CRC.c -o CRC
if (( $? != 0 )); then
    echo Error compiling CRC.c.
    echo Exiting.
    exit 1
fi

NOTREADY=1
while (( $NOTREADY )); do
    echo "Use default CRC generator 0x04C11DB7?"
    read DEF
    
    if [[ $DEF == "no" ]]; then
        echo -n "Input generator > "
        read GEN
        GENFLAG="-g $GEN"
        echo Using generator $GEN
        NOTREADY=0
    elif [[ $DEF == "yes" ]]; then
        NOTREADY=0
    else
        echo "Please enter 'yes' or 'no'."
    fi
done

echo "How many random burst-error trials?"
read TRIALS
echo "How many error-free control trials?"
read CONTROLTRIALS


REF=32
FRAME=1520
CAUGHT=0
MISSES=""
UNDER=0
OVER=0
EQUAL=0
CONTROL=0
EF=0
UF=0
OF=0
CF=0
W=69 # terminal width
i=0
T=$(( $TRIALS + $CONTROLTRIALS ))
PBARSEG=$(( $T / $W + 1 ))
echo -e "This will take a moment...\n"
echo -ne "                                                                      ]\r[>"
for (( i=1; $i < $T; i++ )); do   
    
    (( $i % $PBARSEG == 0 )) && echo -ne "\b=>" &&  W=$(( $W - 1 ))
    
#    echo -ne "\r"
    if (( $i >= $TRIALS )); then
        BURST=0
    else 
        BURST=$(( ($RANDOM % ($REF * 2)) + 1 ))
    fi
    (( $BURST < $REF )) && (( $BURST != 0 )) && UF=$(( $UF + 1 ))
    (( $BURST > $REF )) &&  OF=$(( $OF + 1 ))
    (( $BURST == $REF )) && EF=$(( $EF + 1 ))
    (( $BURST == 0 )) && CF=$(( $CF + 1 ))
    ## Now run the CRC binary
    cat /dev/urandom | tr -dc A-Za-z0-9| head -c $FRAME | ./CRC $GENFLAG 11 -q -e $BURST  
    ## 
    if (( $? != 0 )); then
        CAUGHT=$(( $CAUGHT + 1 ))
        (( $BURST < $REF )) && (( $BURST != 0 )) && \
            UNDER=$(( $UNDER + 1 ))
        (( $BURST > $REF )) && OVER=$(( $OVER + 1 ))
        (( $BURST == $REF )) && EQUAL=$(( $EQUAL + 1 ))
        (( $BURST == 0 )) && CONTROL=$(( $CONTROL + 1 ))
    else
        MISSES="${MISSES} $BURST"
    fi
done
echo -en "\b"
for (( j=0; j < $W; j++ )); do
    echo -n "="
done
echo
echo
(
  echo DETECTED CORRUPTION IN $CAUGHT OF $TRIALS ERROR CASES, AND IN $CONTROL OF $CF
  echo CONTROL CASES. BURSTS WERE BETWEEN 1 AND 64 BITS IN SIZE, 0 FOR 
  echo CONTROL GROUP. MISSED BURSTS OF THE FOLLOWING SIZES:
  echo $MISSES | tr " " "\\n" | sort -n| uniq | tr "\\n" " "
  echo -e "\n"
  echo Note that small bursts may be undetected simply because they failed to
  echo flip any bits. This is also possible for large bursts, but less likely.
  echo 
  echo "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-="
  echo "BURST ERROR LENGTH     |    NUMBER OF FRAMES     |    NUMBER DETECTED"
  echo "-----------------------------------------------------------------------"  
  printf "UNDER %d\t\t\t%d\t\t\t%d\n" $REF $UF $UNDER
  printf "EQUAL TO %d\t\t\t%d\t\t\t%d\n" $REF $EF $EQUAL
  printf "OVER  %d\t\t\t%d\t\t\t%d\n" $REF $OF $OVER
  printf "NO BURST ERROR\t\t\t%d\t\t\t%d\n" $CF $CONTROL
  echo "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-="
) | tee $OUTFILE && echo -e "\nSaved report in $OUTFILE"
   
