#!/usr/local/bin/perl

@mult = (2,2,4,8,16,32,64,128);
@base = (1,33,66,132,264,528,1056,2112,4032);
$shift = shift;
$val[255] = 0;

for$i(0..7) {
   for $j (0..15) {
	  $k = ($i<<4)+$j;
	  $val = (($base[$i] + ($mult[$i] * $j)) >> $shift);
	  $val = 127 if($val > 127);
	  $val[$k] = 128 + $val;
	  $val[255-$k] = 127 - $val;
   }
}

print STDERR "\n";

while(<>) {
	$old = $_;
	s/\000-\377/pack("C",$val[ord($&)])/eg;
	for $i (0..length ($_)-1) {
		substr($_,$i,1) = sprintf("%c",$val[ord(substr($_,$i,1))]);
	}
	print;
}
