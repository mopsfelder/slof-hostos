\ *****************************************************************************
\ * Copyright (c) 2004, 2008 IBM Corporation
\ * All rights reserved.
\ * This program and the accompanying materials
\ * are made available under the terms of the BSD License
\ * which accompanies this distribution, and is available at
\ * http://www.opensource.org/licenses/bsd-license.php
\ *
\ * Contributors:
\ *     IBM Corporation - initial implementation
\ ****************************************************************************/

defer '(r@)
defer '(r!)
1 VALUE /(r)


\ The rest of the code already implemented in prim.in
\ In the end all of this should be moved over there and this file terminated

: (rfill) ( addr size pattern 'r! /r -- )
	to /(r) to '(r!) ff and
	dup 8 lshift or dup 10 lshift or dup 20 lshift or
	-rot bounds ?do dup i '(r!) /(r) +loop drop
;

: rfill ( addr size pattern -- )
	3dup drop or 7 AND CASE
		0 OF ['] rx! /x ENDOF
		4 OF ['] rl! /l ENDOF
		2 OF ['] rw! /w ENDOF
		dup OF ['] rb! /c ENDOF
	ENDCASE (rfill)
;



