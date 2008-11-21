\ *****************************************************************************
\ * Copyright (c) 2004, 2007 IBM Corporation
\ * All rights reserved.
\ * This program and the accompanying materials
\ * are made available under the terms of the BSD License
\ * which accompanies this distribution, and is available at
\ * http://www.opensource.org/licenses/bsd-license.php
\ *
\ * Contributors:
\ *     IBM Corporation - initial implementation
\ ****************************************************************************/

: words
  last @
  begin ?dup while
   dup cell+ char+ count type space @
  repeat
;

: .calls    ( xt -- )
  current-node @ >r 0 set-node    \ only search commands, according too IEEE1275

  last begin @ ?dup while   ( xt currxt )
    dup cell+ char+         ( xt currxt name* )
    dup dup c@ + 1+ aligned ( xt currxt name* CFA )
    dup @ <colon> = IF      ( xt currxt name* CFA )
      begin
        cell+ dup @ ['] semicolon <>
      while		    ( xt currxt *name pos )
        dup @ 4 pick = IF   ( xt currxt *name pos )
	  over count type space
	  begin cell+ dup @ ['] semicolon = until cell - \ eat up other occurences
        THEN
      repeat
    THEN
    2drop ( xt currxt )
  repeat
  drop

  r> set-node		   \ restore node
  ;

0 value #sift-count
false value sift-compl-only

: $inner-sift ( text-addr text-len LFA -- ... word-addr word-len true | false )
  dup cell+ char+ count           \ get word name
  2dup 6 pick 6 pick find-isubstr \ is there a partly match?
  \ in tab completion mode the substring has to be at the beginning
  sift-compl-only IF 0= ELSE over < THEN
  IF
    #sift-count 1+ to #sift-count \ count completions
    true
  ELSE
    2drop false
  THEN
  ;

: $sift    ( text-addr text-len -- )
  current-node @ >r 0 set-node	\ only search commands, according too IEEE1275
  sift-compl-only >r false to sift-compl-only \ all substrings, not only compl.
  last begin @ ?dup while	\ walk the whole dictionary
    $inner-sift IF type space THEN
  repeat
  2drop
  0 to #sift-count	   \ we don't need completions here.
  r> to sift-compl-only    \ restore previous sifting mode
  r> set-node		   \ restore node
  ;

: sifting    ( "text< >" -- )
  parse-word $sift
  ;

