(File |/usr/lib/lisp/auxfns1.l|)
($patom1 lambda patom)
(charcnt lambda nwritn -)
($tocolumn lambda |1+| quote patom eq do nwritn - setq greaterp cond)
($dinc lambda charcnt quote linelength -)
($prd1 lambda print progn printret return quote $patom1 pntlen plus atom |1+| cdr setq null cond car $prdf prog)
($prdf lambda go terpr null cadr cdr $prd1 $dinc equal quote member setq car length $patom1 and print progn printret return charcnt flatc + lessp atom or cond $tocolumn prog)
($prpr lambda $prdf terpr setq quote boundp not cond)
(condclosefile lambda setq close terpr cond)
(pp nlambda cdr $prpr bcdp list boundp getd drain or patom terpr progn msg eq go outfile cadr eval quote equal dtpr return condclosefile car null cond setq prog)
(listp lambda null dtpr or)
(nthrest lambda cdr |1-| nthrest lessp cond)
(nth lambda nthcdr car)
(nthcdr lambda |1-| nthcdr cons lessp bigp not cdr null and zerop cond)
(charcnt lambda nwritn -)
(linelength nlambda setq car numberp null cond)
(printblanks lambda |1-| quote patom lessp do)
(msgmake lambda atom cdr msgmake setq eq and append quote cons list null cond car)
