#!/usr/local/bin/perl

$shift = shift;
$val[255] = 0;
for $i(0..8) { $mx[$i] = 1 << ($i+4); }
$mx[0] = 0;

$l = 0;
for $i(0..127) {
	$z = $i<<$shift;
	$l++ if $z >= $mx[$l+1] && $l < 7;
	
	$val = ($z-$mx[$l])>>($l==0 ?1:$l);
	$val = 15 if($val > 15);
	$val += $l<<4;

	$val[128+$i] =   $val;
	$val[127-$i] = - $val;
	print STDERR "$i:$val ";
}
print STDERR "\n";

while(<>) {
	for $i (0..length ($_)-1) {
		substr($_,$i,1) = sprintf("%c",$val[ord(substr($_,$i,1))]);
	}
	print;
}
