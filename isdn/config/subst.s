	.file	"subst.c"
gcc2_compiled.:
___gnu_compiled_c:
.lcomm _buff.2,32
.text
LC0:
	.ascii "Error code %d\12\0"
	.align 4
_xstrerror:
	pushl %ebp
	movl %esp,%ebp
	cmpl $0,_errno
	jl L2
	movl _errno,%eax
	cmpl %eax,_sys_nerr
	jle L2
	movl _sys_errlist(,%eax,4),%eax
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4,0x90
L2:
	movl _errno,%edx
	pushl %edx
	pushl $LC0
	pushl $_buff.2
	call _sprintf
	movl $_buff.2,%eax
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4
_xstrchr:
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	movb 12(%ebp),%dl
	cmpb $0,(%eax)
	je L6
	.align 2,0x90
L9:
	cmpb %dl,(%eax)
	je L10
	incl %eax
	cmpb $0,(%eax)
	jne L9
L6:
	xorl %eax,%eax
L10:
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4
_xstrrchr:
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%edx
	movb 12(%ebp),%cl
	xorl %eax,%eax
	cmpb $0,(%edx)
	je L13
	.align 2,0x90
L16:
	cmpb %cl,(%edx)
	jne L14
	movl %edx,%eax
L14:
	incl %edx
	cmpb $0,(%edx)
	jne L16
L13:
	movl %ebp,%esp
	popl %ebp
	ret
LC1:
	.ascii "%s: Can't copy \"%s\", %s\12\0"
	.align 4
_xstrdup:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %ebx
	movl 8(%ebp),%ebx
	xorb %al,%al
	movl %ebx,%edi
	cld
	movl $-1,%ecx
	repne
	scasb
	movl %ecx,%eax
	notl %eax
	pushl %eax
	call _malloc
	addl $4,%esp
	testl %eax,%eax
	jne L18
	call _xstrerror
	pushl %eax
	pushl %ebx
	movl _argv0,%edx
	pushl %edx
	pushl $LC1
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L18:
	pushl %ebx
	pushl %eax
	call _strcpy
	leal -8(%ebp),%esp
	popl %ebx
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
LC2:
	.ascii "%s: Can't unlink %s, %s\12\0"
LC3:
	.ascii "%s: Can't link %s to %s, %s\12\0"
	.align 4
_mv:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%esi
	movl 12(%ebp),%ebx
	pushl %ebx
	call _unlink
	addl $4,%esp
	testl %eax,%eax
	jge L20
	cmpl $2,_errno
	je L20
	call _xstrerror
	pushl %eax
	pushl %ebx
	jmp L23
	.align 4,0x90
L20:
	pushl %ebx
	pushl %esi
	call _link
	addl $8,%esp
	testl %eax,%eax
	jge L21
	call _xstrerror
	pushl %eax
	pushl %ebx
	pushl %esi
	movl _argv0,%edx
	pushl %edx
	pushl $LC3
	pushl $__IO_stderr_
	call _fprintf
	jmp L19
	.align 4,0x90
L21:
	pushl %esi
	call _unlink
	addl $4,%esp
	testl %eax,%eax
	jge L19
	call _xstrerror
	pushl %eax
	pushl %esi
L23:
	movl _argv0,%edx
	pushl %edx
	pushl $LC2
	pushl $__IO_stderr_
	call _fprintf
L19:
	leal -8(%ebp),%esp
	popl %ebx
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret
LC4:
	.ascii "r\0"
	.align 4
_cmp:
	pushl %ebp
	movl %esp,%ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl $LC4
	movl 8(%ebp),%edx
	pushl %edx
	call _fopen
	movl %eax,%edi
	addl $8,%esp
	testl %edi,%edi
	jne L25
	movl $1,%eax
	jmp L37
	.align 4,0x90
L25:
	pushl $LC4
	movl 12(%ebp),%edx
	pushl %edx
	call _fopen
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	jne L27
	pushl %edi
	jmp L38
	.align 4,0x90
	.align 2,0x90
L27:
	movl 4(%edi),%eax
	cmpl %eax,8(%edi)
	ja L29
	pushl %edi
	call ___uflow
	addl $4,%esp
	movl %eax,%esi
	jmp L30
	.align 4,0x90
L29:
	movl 4(%edi),%eax
	movzbl (%eax),%esi
	incl 4(%edi)
L30:
	cmpl $-1,%esi
	je L28
	movl 4(%ebx),%eax
	cmpl %eax,8(%ebx)
	ja L32
	pushl %ebx
	call ___uflow
	addl $4,%esp
	cmpl %esi,%eax
	jne L36
	jmp L27
	.align 4,0x90
L32:
	movl 4(%ebx),%eax
	movzbl (%eax),%eax
	incl 4(%ebx)
	cmpl %esi,%eax
	je L27
	jmp L36
	.align 4,0x90
L28:
	movl 4(%ebx),%eax
	cmpl %eax,8(%ebx)
	ja L35
	pushl %ebx
	call ___uflow
	addl $4,%esp
	cmpl $-1,%eax
	jne L36
	jmp L34
	.align 4,0x90
L35:
	movl 4(%ebx),%eax
	movzbl (%eax),%eax
	incl 4(%ebx)
	cmpl $-1,%eax
	je L34
L36:
	pushl %edi
	call _fclose
	pushl %ebx
L38:
	call _fclose
	movl $1,%eax
	jmp L37
	.align 4,0x90
L34:
	pushl %edi
	call _fclose
	pushl %ebx
	call _fclose
	xorl %eax,%eax
L37:
	leal -12(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
	.align 4
_istemplate:
	pushl %ebp
	movl %esp,%ebp
	pushl %ebx
	movl 8(%ebp),%eax
	.align 2,0x90
L40:
	pushl $61
	pushl %eax
	call _xstrchr
	addl $8,%esp
	testl %eax,%eax
	je L46
	cmpb $40,1(%eax)
	jne L42
	cmpb $41,2(%eax)
	jne L42
	cmpb $60,3(%eax)
	je L41
L42:
	incl %eax
	jmp L40
	.align 4,0x90
L41:
	testl %eax,%eax
	jne L44
	jmp L46
	.align 4,0x90
L50:
	movb $10,(%eax)
	movb $0,1(%eax)
	movl %ebx,%eax
	jmp L49
	.align 4,0x90
L44:
	addl $4,%eax
	movl %eax,%ebx
	.align 2,0x90
L45:
	pushl $62
	pushl %eax
	call _xstrchr
	addl $8,%esp
	testl %eax,%eax
	je L46
	cmpb $40,1(%eax)
	jne L47
	cmpb $41,2(%eax)
	jne L47
	cmpb $61,3(%eax)
	je L50
L47:
	incl %eax
	jmp L45
	.align 4,0x90
L46:
	xorl %eax,%eax
L49:
	movl -4(%ebp),%ebx
	movl %ebp,%esp
	popl %ebp
	ret
LC5:
	.ascii "%s: Can't splice %s+%s+%s, %s\12\0"
LC6:
	.ascii "%s%s%s\0"
	.align 4
_splice:
	pushl %ebp
	movl %esp,%ebp
	subl $4,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 12(%ebp),%esi
	movl 16(%ebp),%ebx
	xorb %al,%al
	movl 8(%ebp),%edi
	cld
	movl $-1,%ecx
	repne
	scasb
	movl %ecx,%edx
	notl %edx
	movl %edx,-4(%ebp)
	movl %esi,%edi
	cld
	movl $-1,%ecx
	repne
	scasb
	movl %ecx,%edx
	notl %edx
	movl %ebx,%edi
	cld
	movl $-1,%ecx
	repne
	scasb
	movl %ecx,%eax
	notl %eax
	addl %edx,%eax
	movl -4(%ebp),%ecx
	leal -2(%ecx,%eax),%eax
	pushl %eax
	call _malloc
	movl %eax,%edi
	addl $4,%esp
	testl %edi,%edi
	jne L53
	call _xstrerror
	pushl %eax
	pushl %ebx
	pushl %esi
	movl 8(%ebp),%ecx
	pushl %ecx
	movl _argv0,%ecx
	pushl %ecx
	pushl $LC5
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L53:
	pushl %ebx
	pushl %esi
	movl 8(%ebp),%ecx
	pushl %ecx
	pushl $LC6
	pushl %edi
	call _sprintf
	movl %edi,%eax
	leal -16(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
LC7:
	.ascii "%s:  subst loop in %s:\12\11%s\12\0"
LC8:
	.ascii "%s:  can't write %s, %s\12\0"
	.align 4
_doline:
	pushl %ebp
	movl %esp,%ebp
	subl $8,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl $0,-8(%ebp)
	movl 16(%ebp),%ecx
	pushl %ecx
	call _xstrdup
	movl %eax,-4(%ebp)
	addl $4,%esp
	jmp L67
	.align 4,0x90
	.align 2,0x90
L58:
	movl -4(%ebp),%esi
	.align 2,0x90
L59:
	movl 20(%ebp),%ecx
	movl (%ecx),%eax
	movsbl (%eax),%eax
	pushl %eax
	pushl %esi
	call _xstrchr
	movl %eax,%esi
	addl $8,%esp
	testl %esi,%esi
	je L57
	movl 20(%ebp),%edx
	movl 4(%edx),%edx
	pushl %edx
	movl 20(%ebp),%ecx
	movl (%ecx),%ecx
	pushl %ecx
	pushl %esi
	call _strncmp
	addl $12,%esp
	testl %eax,%eax
	jne L61
	movb (%esi),%bl
	movb $0,(%esi)
	incl -8(%ebp)
	movl %esi,%eax
	movl 20(%ebp),%edx
	addl 4(%edx),%eax
	pushl %eax
	movl 8(%edx),%ecx
	pushl %ecx
	movl -4(%ebp),%edx
	pushl %edx
	call _splice
	movl %eax,%edi
	movb %bl,(%esi)
	movl -4(%ebp),%ecx
	pushl %ecx
	pushl %edi
	call _strcmp
	addl $20,%esp
	testl %eax,%eax
	jne L63
	movl -4(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%ecx
	pushl %ecx
	movl _argv0,%edx
	pushl %edx
	pushl $LC7
	pushl $__IO_stderr_
	call _fprintf
	pushl %edi
	call _free
	addl $24,%esp
	jmp L57
	.align 4,0x90
L63:
	movl -4(%ebp),%ecx
	pushl %ecx
	call _free
	movl %edi,-4(%ebp)
	addl $4,%esp
	jmp L58
	.align 4,0x90
L61:
	incl %esi
	jmp L59
	.align 4,0x90
L57:
	addl $12,20(%ebp)
L67:
	movl 24(%ebp),%edx
	cmpl %edx,20(%ebp)
	jb L58
	cmpl $0,-8(%ebp)
	jle L65
	movl 12(%ebp),%ecx
	pushl %ecx
	movl -4(%ebp),%edx
	pushl %edx
	call _fputs
	addl $8,%esp
	cmpl $-1,%eax
	jne L65
	call _xstrerror
	pushl %eax
	movl 8(%ebp),%ecx
	pushl %ecx
	movl _argv0,%edx
	pushl %edx
	pushl $LC8
	pushl $__IO_stderr_
	call _fprintf
	movl -4(%ebp),%ecx
	pushl %ecx
	call _free
	movl $-1,%eax
	jmp L66
	.align 4,0x90
L65:
	movl -4(%ebp),%edx
	pushl %edx
	call _free
	movl -8(%ebp),%eax
L66:
	leal -20(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
LC9:
	.ascii "substtmp.new\0"
LC10:
	.ascii "substtmp.old\0"
LC11:
	.ascii "%s/substtmp.new\0"
LC12:
	.ascii "%s/substtmp.old\0"
LC13:
	.ascii "%s: can't open %s, %s\12\0"
LC14:
	.ascii "%s: %s exists, cannot proceed\12\0"
LC15:
	.ascii "w\0"
LC16:
	.ascii "%s: cannot create temporaries %s and %s\12\0"
LC17:
	.ascii "%s: Line %d of %s is too long\12\0"
LC18:
	.ascii "%s: %s:%d: unknown variable:\12\11%s\0"
LC19:
	.ascii "%s: can't close %s, %s\12\0"
LC20:
	.ascii "%s: unchanged\12\0"
LC21:
	.ascii "%s: updated\12\0"
	.align 4
_Process:
	pushl %ebp
	movl %esp,%ebp
	subl $3084,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl $47
	movl 8(%ebp),%edx
	pushl %edx
	call _xstrrchr
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	jne L69
	movl LC9,%edx
	movl %edx,-1024(%ebp)
	movl LC9+4,%edx
	movl %edx,-1020(%ebp)
	movl LC9+8,%edx
	movl %edx,-1016(%ebp)
	movb LC9+12,%dl
	movb %dl,-1012(%ebp)
	movl LC10,%edx
	movl %edx,-2048(%ebp)
	movl LC10+4,%edx
	movl %edx,-2044(%ebp)
	movl LC10+8,%edx
	movl %edx,-2040(%ebp)
	movb LC10+12,%dl
	movb %dl,-2036(%ebp)
	jmp L70
	.align 4,0x90
L69:
	movb $0,(%ebx)
	movl 8(%ebp),%edx
	pushl %edx
	pushl $LC11
	leal -1024(%ebp),%eax
	pushl %eax
	call _sprintf
	movl 8(%ebp),%edx
	pushl %edx
	pushl $LC12
	leal -2048(%ebp),%eax
	pushl %eax
	call _sprintf
	movb $47,(%ebx)
	addl $24,%esp
L70:
	pushl $LC4
	movl 8(%ebp),%edx
	pushl %edx
	call _fopen
	movl %eax,-3080(%ebp)
	addl $8,%esp
	testl %eax,%eax
	jne L71
	call _xstrerror
	pushl %eax
	movl 8(%ebp),%edx
	pushl %edx
	movl _argv0,%edx
	pushl %edx
	pushl $LC13
	pushl $__IO_stderr_
	call _fprintf
	jmp L68
	.align 4,0x90
L71:
	pushl $LC4
	leal -1024(%ebp),%esi
	pushl %esi
	call _fopen
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	je L72
	movl -3080(%ebp),%edx
	pushl %edx
	call _fclose
	pushl %esi
	movl _argv0,%edx
	pushl %edx
	pushl $LC14
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L72:
	pushl $LC4
	leal -2048(%ebp),%esi
	pushl %esi
	call _fopen
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	je L73
	pushl %esi
	movl _argv0,%edx
	pushl %edx
	pushl $LC14
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L73:
	pushl $LC15
	leal -2048(%ebp),%eax
	pushl %eax
	call _fopen
	movl %eax,%ebx
	pushl $LC15
	leal -1024(%ebp),%eax
	pushl %eax
	call _fopen
	movl %eax,-3084(%ebp)
	addl $16,%esp
	testl %eax,%eax
	je L75
	testl %ebx,%ebx
	jne L74
L75:
	testl %ebx,%ebx
	je L76
	pushl %ebx
	call _fclose
	addl $4,%esp
L76:
	leal -2048(%ebp),%eax
	pushl %eax
	call _unlink
	addl $4,%esp
	cmpl $0,-3084(%ebp)
	je L77
	movl -3084(%ebp),%edx
	pushl %edx
	call _fclose
	addl $4,%esp
L77:
	leal -1024(%ebp),%ebx
	pushl %ebx
	call _unlink
	pushl %ebx
	leal -2048(%ebp),%eax
	pushl %eax
	movl _argv0,%edx
	pushl %edx
	pushl $LC16
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L90:
	movl 8(%ebp),%edx
	pushl %edx
	pushl %edi
	movl _argv0,%edx
	pushl %edx
	pushl $LC17
	pushl $__IO_stderr_
	call _fprintf
	incl -3076(%ebp)
	addl $20,%esp
	jmp L79
	.align 4,0x90
L91:
	incl -3076(%ebp)
	jmp L79
	.align 4,0x90
L74:
	pushl %ebx
	call _fclose
	movl $1,%edi
	movl $0,-3076(%ebp)
	addl $4,%esp
	leal -3072(%ebp),%esi
	.align 2,0x90
L78:
	movl -3080(%ebp),%edx
	pushl %edx
	pushl $1024
	pushl %esi
	call _fgets
	addl $12,%esp
	testl %eax,%eax
	je L79
	pushl $10
	pushl %esi
	call _xstrchr
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	je L90
	movl -3084(%ebp),%edx
	pushl %edx
	pushl %esi
	call _fputs
	pushl %esi
	call _istemplate
	movl %eax,%ebx
	addl $12,%esp
	testl %ebx,%ebx
	je L80
	movl 16(%ebp),%edx
	pushl %edx
	movl 12(%ebp),%edx
	pushl %edx
	pushl %ebx
	movl -3084(%ebp),%edx
	pushl %edx
	movl 8(%ebp),%edx
	pushl %edx
	call _doline
	addl $20,%esp
	testl %eax,%eax
	jl L91
	jle L84
	movl -3080(%ebp),%edx
	pushl %edx
	pushl $1024
	pushl %esi
	call _fgets
	incl %edi
	addl $12,%esp
	jmp L80
	.align 4,0x90
L84:
	pushl %ebx
	pushl %edi
	movl 8(%ebp),%edx
	pushl %edx
	movl _argv0,%edx
	pushl %edx
	pushl $LC18
	pushl $__IO_stderr_
	call _fprintf
	addl $24,%esp
L80:
	incl %edi
	jmp L78
	.align 4,0x90
L79:
	movl -3080(%ebp),%edx
	pushl %edx
	call _fclose
	movl -3084(%ebp),%edx
	pushl %edx
	call _fflush
	addl $8,%esp
	cmpl $-1,%eax
	je L87
	movl -3084(%ebp),%edx
	pushl %edx
	call _fclose
	addl $4,%esp
	cmpl $-1,%eax
	jne L86
L87:
	call _xstrerror
	pushl %eax
	movl 8(%ebp),%edx
	pushl %edx
	movl _argv0,%edx
	pushl %edx
	pushl $LC19
	pushl $__IO_stderr_
	call _fprintf
	incl -3076(%ebp)
	addl $20,%esp
L86:
	cmpl $0,-3076(%ebp)
	jne L89
	movl 8(%ebp),%edx
	pushl %edx
	leal -1024(%ebp),%eax
	pushl %eax
	call _cmp
	addl $8,%esp
	testl %eax,%eax
	jne L88
L89:
	leal -2048(%ebp),%eax
	pushl %eax
	call _unlink
	leal -1024(%ebp),%eax
	pushl %eax
	call _unlink
	movl 8(%ebp),%edx
	pushl %edx
	pushl $LC20
	call _printf
	jmp L68
	.align 4,0x90
L88:
	pushl $1
	pushl $1
	call _signal
	pushl $1
	pushl $2
	call _signal
	pushl $1
	pushl $15
	call _signal
	leal -2048(%ebp),%ebx
	pushl %ebx
	movl 8(%ebp),%edx
	pushl %edx
	call _mv
	addl $32,%esp
	movl 8(%ebp),%edx
	pushl %edx
	leal -1024(%ebp),%eax
	pushl %eax
	call _mv
	pushl $0
	pushl $1
	call _signal
	pushl $0
	pushl $2
	call _signal
	pushl $0
	pushl $15
	call _signal
	addl $32,%esp
	movl 8(%ebp),%edx
	pushl %edx
	pushl $LC21
	call _printf
	pushl %ebx
	call _unlink
L68:
	leal -3096(%ebp),%esp
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret
LC22:
	.ascii "Usage: %s -f file victims...\12\0"
	.align 4
_Usage:
	pushl %ebp
	movl %esp,%ebp
	movl _argv0,%edx
	pushl %edx
	pushl $LC22
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
.data
_NIL.25:
	.ascii "\0"
.text
LC23:
	.ascii "f:\0"
LC24:
	.ascii "%s: Can't open %s to read it, %s\12\0"
LC25:
	.ascii "%s: Can't allocate %d table elements, %s\12\0"
LC26:
	.ascii "%s: Parameter %s is too long\12\0"
LC27:
	.ascii "@<%s>@\0"
	.align 4
.globl _main
_main:
	pushl %ebp
	movl %esp,%ebp
	subl $1180,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%ebx
	call ___main
	movl $0,-1156(%ebp)
	movl 12(%ebp),%edi
	movl (%edi),%edi
	movl %edi,_argv0
	.align 2,0x90
L94:
	pushl $LC23
	movl 12(%ebp),%eax
	pushl %eax
	pushl %ebx
	call _getopt
	movl %eax,%esi
	addl $12,%esp
	cmpl $-1,%esi
	je L95
	cmpl $102,%esi
	je L98
	call _Usage
L98:
	cmpl $0,-1156(%ebp)
	je L99
	call _Usage
L99:
	movl _optarg,%edi
	movl %edi,-1156(%ebp)
	jmp L94
	.align 4,0x90
L95:
	movl _optind,%eax
	sall $2,%eax
	movl %eax,-1176(%ebp)
	addl %eax,12(%ebp)
	pushl $LC4
	movl -1156(%ebp),%edi
	pushl %edi
	call _fopen
	movl %eax,-1160(%ebp)
	addl $8,%esp
	testl %eax,%eax
	jne L101
	call _xstrerror
	movl %eax,-1176(%ebp)
	pushl %eax
	pushl %edi
	movl _argv0,%eax
	pushl %eax
	pushl $LC24
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L101:
	xorl %esi,%esi
	leal -1024(%ebp),%ebx
	.align 2,0x90
L102:
	movl -1160(%ebp),%edi
	pushl %edi
	pushl $1024
	pushl %ebx
	call _fgets
	movl %eax,-1176(%ebp)
	addl $12,%esp
	testl %eax,%eax
	je L103
	incl %esi
	jmp L102
	.align 4,0x90
L103:
	leal (%esi,%esi,2),%eax
	sall $2,%eax
	pushl %eax
	call _malloc
	movl %eax,-1164(%ebp)
	addl $4,%esp
	testl %eax,%eax
	jne L105
	call _xstrerror
	movl %eax,-1176(%ebp)
	pushl %eax
	pushl %esi
	movl _argv0,%eax
	pushl %eax
	pushl $LC25
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L105:
	pushl $0
	pushl $0
	movl -1160(%ebp),%edi
	pushl %edi
	call _fseek
	movl $1,%esi
	movl -1164(%ebp),%eax
	movl %eax,-1168(%ebp)
	addl $12,%esp
	leal -1024(%ebp),%edi
	movl %edi,-1172(%ebp)
	.align 2,0x90
L106:
	movl -1160(%ebp),%eax
	pushl %eax
	pushl $1024
	movl -1172(%ebp),%edi
	pushl %edi
	call _fgets
	movl %eax,-1176(%ebp)
	addl $12,%esp
	testl %eax,%eax
	je L107
	pushl $10
	pushl %edi
	call _xstrchr
	movl %eax,-1180(%ebp)
	addl $8,%esp
	testl %eax,%eax
	jne L109
	movl -1156(%ebp),%eax
	pushl %eax
	pushl %esi
	movl _argv0,%edi
	pushl %edi
	pushl $LC17
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L109:
	movl -1180(%ebp),%eax
	movb $0,(%eax)
	cmpb $0,-1024(%ebp)
	je L108
	cmpb $35,-1024(%ebp)
	je L108
	leal -1024(%ebp),%edi
	movl %edi,-1180(%ebp)
	cmpb $32,-1024(%ebp)
	je L114
	cmpb $9,-1024(%ebp)
	jne L113
	.align 2,0x90
L114:
	incl -1180(%ebp)
	movl -1180(%ebp),%eax
	cmpb $32,(%eax)
	je L114
	cmpb $9,(%eax)
	je L114
L113:
	movl -1180(%ebp),%edi
	cmpb $0,(%edi)
	je L108
	leal -1024(%ebp),%eax
	movl %eax,-1180(%ebp)
	cmpb $0,-1024(%ebp)
	je L139
	cmpb $32,-1024(%ebp)
	je L119
	cmpb $9,-1024(%ebp)
	je L119
	.align 2,0x90
L121:
	incl -1180(%ebp)
	movl -1180(%ebp),%edi
	cmpb $0,(%edi)
	je L139
	cmpb $32,(%edi)
	je L119
	cmpb $9,(%edi)
	jne L121
L119:
	movl -1180(%ebp),%eax
	cmpb $0,(%eax)
	jne L122
L139:
	movl -1168(%ebp),%edi
	movl $_NIL.25,8(%edi)
	jmp L123
	.align 4,0x90
L122:
	movl -1180(%ebp),%eax
	movb $0,(%eax)
	incl %eax
	movl %eax,-1180(%ebp)
	cmpb $0,(%eax)
	je L125
	.align 2,0x90
L128:
	movl -1180(%ebp),%edi
	cmpb $32,(%edi)
	je L126
	cmpb $9,(%edi)
	jne L125
L126:
	incl -1180(%ebp)
	movl -1180(%ebp),%eax
	cmpb $0,(%eax)
	jne L128
L125:
	movl -1180(%ebp),%edi
	pushl %edi
	call _xstrdup
	movl %eax,-1176(%ebp)
	movl -1168(%ebp),%edi
	movl %eax,8(%edi)
	movl -1176(%ebp),%ecx
	movl %ecx,-1180(%ebp)
	addl $4,%esp
	cmpb $0,(%ecx)
	je L130
	.align 2,0x90
L134:
	movl %ecx,%ebx
	incl %ecx
	movl -1180(%ebp),%eax
	cmpb $92,(%eax)
	jne L132
	cmpb $0,1(%eax)
	je L132
	incl %eax
	movl %eax,-1180(%ebp)
	movb (%eax),%dl
	jmp L133
	.align 4,0x90
L132:
	movl -1180(%ebp),%edi
	movb (%edi),%dl
L133:
	movb %dl,(%ebx)
	incl -1180(%ebp)
	movl -1180(%ebp),%eax
	cmpb $0,(%eax)
	jne L134
L130:
	movb $0,(%ecx)
L123:
	xorb %al,%al
	movl -1172(%ebp),%edi
	cld
	movl $-1,%ecx
	repne
	scasb
	movl %ecx,%eax
	notl %eax
	subl $1,%eax
	cmpl $124,%eax
	jbe L135
	movl -1172(%ebp),%edi
	pushl %edi
	movl _argv0,%eax
	pushl %eax
	pushl $LC26
	pushl $__IO_stderr_
	call _fprintf
	pushl $1
	call _exit
	.align 4,0x90
L135:
	movl -1172(%ebp),%edi
	pushl %edi
	pushl $LC27
	leal -1152(%ebp),%ebx
	pushl %ebx
	call _sprintf
	pushl %ebx
	call _xstrdup
	movl %eax,-1176(%ebp)
	movl -1168(%ebp),%edi
	movl %eax,(%edi)
	xorb %dl,%dl
	movl -1176(%ebp),%ebx
	movl %ebx,%edi
	movb %dl,%al
	cld
	movl $-1,%ecx
	repne
	scasb
	notl %ecx
	movl %ecx,%edi
	subl $1,%edi
	movl -1168(%ebp),%eax
	movl %edi,4(%eax)
	addl $12,%eax
	movl %eax,-1168(%ebp)
	addl $16,%esp
L108:
	incl %esi
	jmp L106
	.align 4,0x90
L107:
	movl -1160(%ebp),%eax
	pushl %eax
	call _fclose
	addl $4,%esp
	jmp L140
	.align 4,0x90
	.align 2,0x90
L138:
	movl -1168(%ebp),%eax
	pushl %eax
	movl -1164(%ebp),%edi
	pushl %edi
	movl 12(%ebp),%eax
	movl (%eax),%eax
	pushl %eax
	addl $4,12(%ebp)
	call _Process
	addl $12,%esp
L140:
	movl 12(%ebp),%edi
	cmpl $0,(%edi)
	jne L138
	pushl $0
	call _exit
	.align 4,0x90
.lcomm _argv0,4
