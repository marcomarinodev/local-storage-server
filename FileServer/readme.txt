Client.C ==> i client vengono generati da bash script
Server.C ==> fa da server


Server.C: thread sono gli worker e ce sono n. 
	Prima domanda: Come gestiamo le richieste avendo n workers fissati ?
		=> Abbiamo bisogno di un dispatcher che crea gli n workers,


4 workers: A B C D

A: sta lavorando su 01, B: sta lavorando su 02, C: sta lavorando su 03, D: sta lavorando su 04

Arriva la richiesta dal client 05. Come gestiamo questa cosa?

-----

Seconda domanda: Come gestiamo il dispatcher?

-----

Terza domanda: Come gestiamo il file config

n.workers
dim massima del server (MB)
socket path
n. max di file
directory su cui salvare i file ???