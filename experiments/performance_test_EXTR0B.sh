#!/bin/bash

# This directory:
DIR=`dirname $0`/

# Time limit in seconds:
TIME_LIMIT=10000
# Memory limit in kB:
MEMORY_LIMIT=8000000
REAL_ONLY=0

COMMAND="../tool/build/src/demiurge-bin -b load -s min_api -q blo_dep_api -c learn -m 0 --print=L"

# Maybe change the following line to point to GNU time:
GNU_TIME="/usr/bin/time"
MODEL_CHECKER="$DEMIURGETP/lingeling/blimc"
IC3_CHECKER="$DEMIURGETP/ic3/IC3"
SYNT_CHECKER="$DEMIURGETP/syntactic_checker.py"

# The directory where the benchmarks are located:
BM_DIR="${DIR}benchmarks/"

REAL=10
UNREAL=20

# The benchmarks to be used.
# The files have to be located in ${BM_DIR}.
FILES=(
# unr                       $UNREAL
# eq1                       $REAL
# ex1                       $REAL
# ex2                       $REAL
add2n                     $REAL
add2y                     $REAL
add4n                     $REAL
add4y                     $REAL
add6n                     $REAL
add6y                     $REAL
add8n                     $REAL
add8y                     $REAL
add10n                    $REAL
add10y                    $REAL
add12n                    $REAL
add12y                    $REAL
add14n                    $REAL
add14y                    $REAL
#add16n                    $REAL
#add16y                    $REAL
#add18n                    $REAL
#add18y                    $REAL
#add20n                    $REAL
#add20y                    $REAL
cnt2n                     $REAL
cnt2y                     $REAL
cnt3n                     $REAL
cnt3y                     $REAL
cnt4n                     $REAL
cnt4y                     $REAL
cnt5n                     $REAL
cnt5y                     $REAL
cnt6n                     $REAL
cnt6y                     $REAL
cnt7n                     $REAL
cnt7y                     $REAL
cnt8n                     $REAL
cnt8y                     $REAL
cnt9n                     $REAL
cnt9y                     $REAL
cnt10n                    $REAL
cnt10y                    $REAL
cnt11n                    $REAL
cnt11y                    $REAL
cnt15n                    $REAL
cnt15y                    $REAL
cnt20n                    $REAL
cnt20y                    $REAL
cnt25n                    $REAL
cnt25y                    $REAL
cnt30n                    $REAL
cnt30y                    $REAL
mv2n                      $REAL
mv2y                      $REAL
mvs2n                     $REAL
mvs2y                     $REAL
mv4n                      $REAL
mv4y                      $REAL
mvs4n                     $REAL
mvs4y                     $REAL
mv8n                      $REAL
mv8y                      $REAL
mvs8n                     $REAL
mvs8y                     $REAL
mv12n                     $REAL
mv12y                     $REAL
mvs12n                    $REAL
mvs12y                    $REAL
mv16n                     $REAL
mv16y                     $REAL
mvs16n                    $REAL
mvs16y                    $REAL
mvs18n                    $REAL
mvs18y                    $REAL
mv20n                     $REAL
mv20y                     $REAL
mvs20n                    $REAL
mvs20y                    $REAL
mvs22n                    $REAL
mvs22y                    $REAL
mvs24n                    $REAL
mvs24y                    $REAL
mvs28n                    $REAL
mvs28y                    $REAL
mult2                     $REAL
mult4                     $REAL
mult5                     $REAL
mult6                     $REAL
mult7                     $REAL
mult8                     $REAL
mult9                     $REAL
#mult10                    $REAL
#mult11                    $REAL
#mult12                    $REAL
#mult13                    $REAL
# mult14                    $REAL
# mult15                    $REAL
# mult16                    $REAL
bs8n                      $REAL
bs8y                      $REAL
bs16n                     $REAL
bs16y                     $REAL
bs32n                     $REAL
bs32y                     $REAL
bs64n                     $REAL
bs64y                     $REAL
bs128n                    $REAL
bs128y                    $REAL
stay2n                    $REAL
stay2y                    $REAL
stay4n                    $REAL
stay4y                    $REAL
stay6n                    $REAL
stay6y                    $REAL
stay8n                    $REAL
stay8y                    $REAL
stay10n                   $REAL
stay10y                   $REAL
stay12n                   $REAL
stay12y                   $REAL
stay14n                   $REAL
stay14y                   $REAL
stay16n                   $REAL
stay16y                   $REAL
stay18n                   $REAL
stay18y                   $REAL
stay20n                   $REAL
stay20y                   $REAL
stay22n                   $REAL
stay22y                   $REAL
stay24n                   $REAL
stay24y                   $REAL
#
# genbuf1c2unrealn          $UNREAL
# genbuf1c2unrealy          $UNREAL
# genbuf1c3n                $REAL
genbuf1c3y                $REAL
# genbuf2c2unrealn          $UNREAL
# genbuf2c2unrealy          $UNREAL
# genbuf2c3n                $REAL
genbuf2c3y                $REAL
# genbuf3c2unrealn          $UNREAL
# genbuf3c2unrealy          $UNREAL
# genbuf3c3n                $REAL
genbuf3c3y                $REAL
# genbuf4c2unrealn          $UNREAL
# genbuf4c2unrealy          $UNREAL
# genbuf4c3n                $REAL
genbuf4c3y                $REAL
# genbuf5c2unrealn          $UNREAL
# genbuf5c2unrealy          $UNREAL
# genbuf5c3n                $REAL
genbuf5c3y                $REAL
# genbuf6c2unrealn          $UNREAL
# genbuf6c2unrealy          $UNREAL
# genbuf6c3n                $REAL
genbuf6c3y                $REAL
# genbuf7c2unrealn          $UNREAL
# genbuf7c2unrealy          $UNREAL
# genbuf7c3n                $REAL
# genbuf7c3y                $REAL
# genbuf8c2unrealn          $UNREAL
# genbuf8c2unrealy          $UNREAL
# genbuf8c3n                $REAL
# genbuf8c3y                $REAL
# genbuf9c2unrealn          $UNREAL
# genbuf9c2unrealy          $UNREAL
# genbuf9c3n                $REAL
# genbuf9c3y                $REAL
# genbuf10c2unrealn         $UNREAL
# genbuf10c2unrealy         $UNREAL
# genbuf10c3n               $REAL
# genbuf10c3y               $REAL
# genbuf11c2unrealn         $UNREAL
# genbuf11c2unrealy         $UNREAL
# genbuf11c3n               $REAL
# genbuf11c3y               $REAL
# genbuf12c2unrealn         $UNREAL
# genbuf12c2unrealy         $UNREAL
# genbuf12c3n               $REAL
# genbuf12c3y               $REAL
# genbuf13c2unrealn         $UNREAL
# genbuf13c2unrealy         $UNREAL
# genbuf13c3n               $REAL
# genbuf13c3y               $REAL
# genbuf14c2unrealn         $UNREAL
# genbuf14c2unrealy         $UNREAL
# genbuf14c3n               $REAL
# genbuf14c3y               $REAL
# genbuf15c2unrealn         $UNREAL
# genbuf15c2unrealy         $UNREAL
# genbuf15c3n               $REAL
# genbuf15c3y               $REAL
# genbuf16c2unrealn         $UNREAL
# genbuf16c2unrealy         $UNREAL
# genbuf16c3n               $REAL
# genbuf16c3y               $REAL
#
# genbuf1b3unrealn          $UNREAL
# genbuf1b3unrealy          $UNREAL
# genbuf1b4n                $REAL
genbuf1b4y                $REAL
# genbuf2b3unrealn          $UNREAL
# genbuf2b3unrealy          $UNREAL
# genbuf2b4n                $REAL
genbuf2b4y                $REAL
# genbuf3b3unrealn          $UNREAL
# genbuf3b3unrealy          $UNREAL
# genbuf3b4n                $REAL
genbuf3b4y                $REAL
# genbuf4b3unrealn          $UNREAL
# genbuf4b3unrealy          $UNREAL
# genbuf4b4n                $REAL
genbuf4b4y                $REAL
# genbuf5b3unrealn          $UNREAL
# genbuf5b3unrealy          $UNREAL
# genbuf5b4n                $REAL
genbuf5b4y                $REAL
# genbuf6b3unrealn          $UNREAL
# genbuf6b3unrealy          $UNREAL
# genbuf6b4n                $REAL
genbuf6b4y                $REAL
# genbuf7b3unrealn          $UNREAL
# genbuf7b3unrealy          $UNREAL
# genbuf7b4n                $REAL
# genbuf7b4y                $REAL
# genbuf8b3unrealn          $UNREAL
# genbuf8b3unrealy          $UNREAL
# genbuf8b4n                $REAL
# genbuf8b4y                $REAL
# genbuf9b3unrealn          $UNREAL
# genbuf9b3unrealy          $UNREAL
# genbuf9b4n                $REAL
# genbuf9b4y                $REAL
# genbuf10b3unrealn         $UNREAL
# genbuf10b3unrealy         $UNREAL
# genbuf10b4n               $REAL
# genbuf10b4y               $REAL
# genbuf11b3unrealn         $UNREAL
# genbuf11b3unrealy         $UNREAL
# genbuf11b4n               $REAL
# genbuf11b4y               $REAL
# genbuf12b3unrealn         $UNREAL
# genbuf12b3unrealy         $UNREAL
# genbuf12b4n               $REAL
# genbuf12b4y               $REAL
# genbuf13b3unrealn         $UNREAL
# genbuf13b3unrealy         $UNREAL
# genbuf13b4n               $REAL
# genbuf13b4y               $REAL
# genbuf14b3unrealn         $UNREAL
# genbuf14b3unrealy         $UNREAL
# genbuf14b4n               $REAL
# genbuf14b4y               $REAL
# genbuf15b3unrealn         $UNREAL
# genbuf15b3unrealy         $UNREAL
# genbuf15b4n               $REAL
# genbuf15b4y               $REAL
# genbuf16b3unrealn         $UNREAL
# genbuf16b3unrealy         $UNREAL
# genbuf16b4n               $REAL
# genbuf16b4y               $REAL
#
# genbuf1f3unrealn          $UNREAL
# genbuf1f3unrealy          $UNREAL
# genbuf1f4n                $REAL
genbuf1f4y                $REAL
# genbuf2f3unrealn          $UNREAL
# genbuf2f3unrealy          $UNREAL
# genbuf2f4n                $REAL
genbuf2f4y                $REAL
# genbuf3f3unrealn          $UNREAL
# genbuf3f3unrealy          $UNREAL
# genbuf3f4n                $REAL
genbuf3f4y                $REAL
# genbuf4f3unrealn          $UNREAL
# genbuf4f3unrealy          $UNREAL
# genbuf4f4n                $REAL
genbuf4f4y                $REAL
# genbuf5f4unrealn          $UNREAL
# genbuf5f4unrealy          $UNREAL
# genbuf5f5n                $REAL
genbuf5f5y                $REAL
# genbuf6f5unrealn          $UNREAL
# genbuf6f5unrealy          $UNREAL
# genbuf6f6n                $REAL
genbuf6f6y                $REAL
# genbuf7f6unrealn          $UNREAL
# genbuf7f6unrealy          $UNREAL
# genbuf7f7n                $REAL
# genbuf7f7y                $REAL
# genbuf8f7unrealn          $UNREAL
# genbuf8f7unrealy          $UNREAL
# genbuf8f8n                $REAL
# genbuf8f8y                $REAL
# genbuf9f8unrealn          $UNREAL
# genbuf9f8unrealy          $UNREAL
# genbuf9f9n                $REAL
# genbuf9f9y                $REAL
# genbuf10f9unrealn         $UNREAL
# genbuf10f9unrealy         $UNREAL
# genbuf10f10n              $REAL
# genbuf10f10y              $REAL
# genbuf11f10unrealn        $UNREAL
# genbuf11f10unrealy        $UNREAL
# genbuf11f11n              $REAL
# genbuf11f11y              $REAL
# genbuf12f11unrealn        $UNREAL
# genbuf12f11unrealy        $UNREAL
# genbuf12f12n              $REAL
# genbuf12f12y              $REAL
# genbuf13f12unrealn        $UNREAL
# genbuf13f12unrealy        $UNREAL
# genbuf13f13n              $REAL
# genbuf13f13y              $REAL
# genbuf14f13unrealn        $UNREAL
# genbuf14f13unrealy        $UNREAL
# genbuf14f14n              $REAL
# genbuf14f14y              $REAL
# genbuf15f14unrealn        $UNREAL
# genbuf15f14unrealy        $UNREAL
# genbuf15f15n              $REAL
# genbuf15f15y              $REAL
# genbuf16f15unrealn        $UNREAL
# genbuf16f15unrealy        $UNREAL
# genbuf16f16n              $REAL
# genbuf16f16y              $REAL
#
#
# amba2c6unrealn            $UNREAL
# amba2c6unrealy            $UNREAL
# amba2c7n                  $REAL
amba2c7y                  $REAL
# amba3c4unrealn            $UNREAL
# amba3c4unrealy            $UNREAL
# amba3c5n                  $REAL
amba3c5y                  $REAL
# amba4c6unrealn            $UNREAL
# amba4c6unrealy            $UNREAL
# amba4c7n                  $REAL
amba4c7y                  $REAL
# amba5c4unrealn            $UNREAL
# amba5c4unrealy            $UNREAL
# amba5c5n                  $REAL
# amba5c5y                  $REAL
# amba6c4unrealn            $UNREAL
# amba6c4unrealy            $UNREAL
# amba6c5n                  $REAL
# amba6c5y                  $REAL
# amba7c4unrealn            $UNREAL
# amba7c4unrealy            $UNREAL
# amba7c5n                  $REAL
# amba7c5y                  $REAL
# amba8c6unrealn            $UNREAL
# amba8c6unrealy            $UNREAL
# amba8c7n                  $REAL
# amba8c7y                  $REAL
# amba9c4unrealn            $UNREAL
# amba9c4unrealy            $UNREAL
# amba9c5n                  $REAL
# amba9c5y                  $REAL
# amba10c4unrealn           $UNREAL
# amba10c4unrealy           $UNREAL
# amba10c5n                 $REAL
# amba10c5y                 $REAL
#
# amba2b8unrealn            $UNREAL
# amba2b8unrealy            $UNREAL
# amba2b9n                  $REAL
amba2b9y                  $REAL
# amba3b4unrealn            $UNREAL
# amba3b4unrealy            $UNREAL
# amba3b5n                  $REAL
amba3b5y                  $REAL
# amba4b8unrealn            $UNREAL
# amba4b8unrealy            $UNREAL
# amba4b9n                  $REAL
amba4b9y                  $REAL
# amba5b4unrealn            $UNREAL
# amba5b4unrealy            $UNREAL
# amba5b5n                  $REAL
# amba5b5y                  $REAL
# amba6b4unrealn            $UNREAL
# amba6b4unrealy            $UNREAL
# amba6b5n                  $REAL
# amba6b5y                  $REAL
# amba7b4unrealn            $UNREAL
# amba7b4unrealy            $UNREAL
# amba7b5n                  $REAL
# amba7b5y                  $REAL
# amba8b5unrealn            $UNREAL
# amba8b5unrealy            $UNREAL
# amba8b6n                  $REAL
# amba8b6y                  $REAL
# amba9b4unrealn            $UNREAL
# amba9b4unrealy            $UNREAL
# amba9b5n                  $REAL
# amba9b5y                  $REAL
# amba10b4unrealn           $UNREAL
# amba10b4unrealy           $UNREAL
# amba10b5n                 $REAL
# amba10b5y                 $REAL
#
# amba2f8unrealn            $UNREAL
# amba2f8unrealy            $UNREAL
# amba2f9n                  $REAL
amba2f9y                  $REAL
# amba3f8unrealn            $UNREAL
# amba3f8unrealy            $UNREAL
# amba3f9n                  $REAL
amba3f9y                  $REAL
# amba4f24unrealn           $UNREAL
# amba4f24unrealy           $UNREAL
# amba4f25n                 $REAL
# amba4f25y                 $REAL
# amba5f16unrealn           $UNREAL
# amba5f16unrealy           $UNREAL
# amba5f17n                 $REAL
# amba5f17y                 $REAL
# amba6f20unrealn           $UNREAL
# amba6f20unrealy           $UNREAL
# amba6f21n                 $REAL
# amba6f21y                 $REAL
# amba7f24unrealn           $UNREAL
# amba7f24unrealy           $UNREAL
# amba7f25n                 $REAL
# amba7f25y                 $REAL
# amba8f56unrealn           $UNREAL
# amba8f56unrealy           $UNREAL
# amba8f57n                 $REAL
# amba8f57y                 $REAL
# amba9f32unrealn           $UNREAL
# amba9f32unrealy           $UNREAL
# amba9f33n                 $REAL
# amba9f33y                 $REAL
# amba10f36unrealn          $UNREAL
# amba10f36unrealy          $UNREAL
# amba10f37n                $REAL
# amba10f37y                $REAL
  )

TIMESTAMP=`date +%s`
RES_TXT_FILE="${DIR}results/results_EXTR0B_${TIMESTAMP}.txt"
RES_DIR="${DIR}results/results_EXTR0B_${TIMESTAMP}/"
mkdir -p "${DIR}results/"
mkdir -p ${RES_DIR}

ulimit -m ${MEMORY_LIMIT} -v ${MEMORY_LIMIT} -t ${TIME_LIMIT}
for element in $(seq 0 2 $((${#FILES[@]} - 1)))
do
     file_name=${FILES[$element]}
     infile_path=${BM_DIR}${file_name}.aag
     outfile_path=${RES_DIR}${file_name}_synth.aag
     correct_real=${FILES[$element+1]}
     echo "Synthesizing ${file_name}.aag ..."
     echo "=====================  $file_name.aag =====================" 1>> $RES_TXT_FILE

     #------------------------------------------------------------------------------
     # BEGIN execution of synthesis tool
     echo " Running the synthesizer ... "
     ${GNU_TIME} --quiet --output=${RES_TXT_FILE} -a -f "Synthesis time: %e sec (Real time) / %U sec (User CPU time)" ${COMMAND} -i $infile_path -o  $outfile_path >> ${RES_TXT_FILE}
     exit_code=$?
     echo "  Done running the synthesizer. "
     # END execution of synthesis tool

     if [[ $exit_code == 137 ]];
     then
         echo "  Timeout!"
         echo "Timeout: 1" 1>> $RES_TXT_FILE
         continue
     else
         echo "Timeout: 0" 1>> $RES_TXT_FILE
     fi

     if [[ $exit_code != $REAL && $exit_code != $UNREAL ]];
     then
         echo "  Strange exit code: $exit_code (crash or out-of-memory)!"
         echo "Crash or out-of-mem: 1 (Exit code: $exit_code)" 1>> $RES_TXT_FILE
         continue
     else
         echo "Crash or out-of-mem: 0" 1>> $RES_TXT_FILE
     fi

     #------------------------------------------------------------------------------
     # BEGIN analyze realizability verdict
     if [[ $exit_code == $REAL && $correct_real == $UNREAL ]];
     then
         echo "  ERROR: Tool reported 'realizable' for an unrealizable spec!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         echo "Realizability correct: 0 (tool reported 'realizable' instead of 'unrealizable')" 1>> $RES_TXT_FILE
         continue
     fi
     if [[ $exit_code == $UNREAL && $correct_real == $REAL ]];
     then
         echo "  ERROR: Tool reported 'unrealizable' for a realizable spec!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
         echo "Realizability correct: 0 (tool reported 'unrealizable' instead of 'realizable')" 1>> $RES_TXT_FILE
         continue
     fi
     if [[ $exit_code == $UNREAL ]];
     then
         echo "  The spec has been correctly identified as 'unrealizable'."
         echo "Realizability correct: 1 (unrealizable)" 1>> $RES_TXT_FILE
     else
         echo "  The spec has been correctly identified as 'realizable'."
         echo "Realizability correct: 1 (realizable)" 1>> $RES_TXT_FILE

         if [[ $REAL_ONLY == 1 ]];
         then
            continue
         fi
         # END analyze realizability verdict

         #------------------------------------------------------------------------------
         # BEGIN syntactic check
         echo " Checking the synthesis result syntactically ... "
         if [ -f $outfile_path ];
         then
             echo "  Output file has been created."
             python $SYNT_CHECKER $infile_path $outfile_path
             exit_code=$?
             if [[ $exit_code == 0 ]];
             then
               echo "  Output file is OK syntactically."
               echo "Output file OK: 1" 1>> $RES_TXT_FILE
             else
               echo "  Output file is NOT OK syntactically."
               echo "Output file OK: 0" 1>> $RES_TXT_FILE
             fi
         else
             echo "  Output file has NOT been created!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
             echo "Output file OK: 0 (no output file created)" 1>> $RES_TXT_FILE
             continue
         fi
         # TODO: perform syntactic check here.
         # END syntactic check

         #------------------------------------------------------------------------------
         # BEGIN model checking
         echo -n " Model checking the synthesis result with BLIMC ... "
         ${GNU_TIME} --quiet --output=${RES_TXT_FILE} -a -f "BLIMC Model-checking time: %e sec (Real time) / %U sec (User CPU time)" $MODEL_CHECKER $outfile_path > /dev/null 2>&1
         check_res=$?
         echo " done. "
         if [[ $check_res == 20 ]];
         then
             echo "  BLIMC-Model-checking was successful."
             echo "BLIMC-Model-checking: 1" 1>> $RES_TXT_FILE
         else
             echo "  BLIMC-Model-checking the resulting circuit failed!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
             echo "BLIMC-Model-checking: 0 (exit code: $check_res)" 1>> $RES_TXT_FILE
         fi
         echo -n " Model checking the synthesis result with IC3 ... "
         ${GNU_TIME} --quiet --output=${RES_TXT_FILE} -a -f "IC3 Model-checking time: %e sec (Real time) / %U sec (User CPU time)" $IC3_CHECKER < $outfile_path > /dev/null 2>&1
         check_res=$?
         echo " done. "
         if [[ $check_res == 20 ]];
         then
             echo "  IC3-Model-checking was successful."
             echo "IC3-Model-checking: 1" 1>> $RES_TXT_FILE
         else
             echo "  IC3-Model-checking the resulting circuit failed!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
             echo "IC3-Model-checking: 0 (exit code: $check_res)" 1>> $RES_TXT_FILE
         fi
         # END end checking

         #------------------------------------------------------------------------------
         # BEGIN determining circuit size
         aig_header_in=$(head -n 1 $infile_path)
         aig_header_out=$(head -n 1 $outfile_path)
         echo "Raw AIGER input size: $aig_header_in" 1>> $RES_TXT_FILE
         echo "Raw AIGER output size: $aig_header_out" 1>> $RES_TXT_FILE
         # END determining circuit size
     fi
done
