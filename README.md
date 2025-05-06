## Cristian-Ioan-George CHIRA, grupa 322CD - descriere rezolvare tema 2 PCom

Pentru a implementa aceasta tema am ales sa folosesc limbajul C++, deoarece ofera
implementari pentru structuri de date (am folosit `std::vector` pentru vectori
alocati dinamic, `std::unordered_map` si `std::unordered_set` care implementeaza
tabele de dispersie).

Fisierele continute sunt `server.cpp`, `subscriber.cpp`, `utils.cpp`, `utils.hpp`
si `Makefile` pentru compilare.

## Functionarea serverului

Serverul incepe prin verificarea parametrului dat. Acesta trebuie sa fie un numar
care reprezinta un port neprivilegiat (intre 1024 si 65535). Dupa care, deschide
un socket UDP pentru a putea primi datagrame cu mesaje de la serverele UDP.
Apoi deschide un socket TCP pentru a putea prelua cererile de conectare de la
clientii TCP. Dupa care, este pregatit polling-ul, adaugand, pentru inceput,
la acesta `stdin` (pentru a putea introduce comanda `exit`), socket-ul UDP si
listner-ul TCP.

Dupa aceasta este declarata o tabela de dispersie avand ca si chei nume si ca
valori date despre abonati, in structura `subscriber_data`, care tine minte
daca este clientul conectat in momentul respectiv, pe ce socket este conectat
si o alta tabela de dispersie ce cuprinde topic-urile la care este abonat. Apoi
avem o alta tabela de dispersie ce are ca si chei file-descriptorii pentru
socket-urile subscriberilor conectati si ca valori numele lor (pentru a putea
gasi rapid in cealalta tabela de dispersie daca exista alt client cu acelasi
ID). Apoi creez o coada in care voi pastra perechi intre topicuri si mesajele primite,
pentru a putea fi expediate clientilor TCP.

Apoi, la fiecare iteratie server-ul face poll intre stdin si toate socket-urile
deschise. Intai verifica daca s-a introdus comanda `exit`, dupa care incearca sa
primeasca un pachet UDP. 

Pentru a primi un pachet UDP se umple un buffer cu 0-uri si se primeste pachetul in acesta,
cu o dimensiune de pana la 1551 octeti. Apoi se construieste alta structura,
`message_with_header` in care pun adresa IP a sender-ului si portul corespunzator,
urmat de mesajul asa cum a fost primit in sine.

Dupa care, server-ul accepta noi conexiuni. Pentru aceasta, imediat ce primeste
conexiunea, se primeste si ID-ul sau, ce este verificat daca exista deja in
tabela de dispersie, caz in care este dat afara. Dupa aceasta, este marcat
ca si conectat in aceasta tabela, completandu-se si socket-ul corespunzator si
este introdus si in tabela socket-client ID. Dupa care, este introdus in vectorul
de `pollfd`-uri, pentru a i se putea face poll, iar apoi este afisat un mesaj
corespunzator in terminal.

Apoi, se itereaza prin toti clientii TCP conectati. Pentru fiecare se verifica
daca acesta s-a deconectat (caz in care se actioneaza in consecinta, eliminandu-l
din poll, marcandu-l ca deconectat , scotandu-l din tabela cu clienti conectati
si afisand un mesaj corespunzator). Daca s-au primit bytes, atunci aceasta ar
trebui sa fie o cerere de abonare/dezabonare, fapt marcat in primul octet trimis.
In urma acestei cereri, clientului i se va trimite inapoi un mesaj de confirmare
(acelasi pachet, doar ca cu primul octet modificat corespunzator), prin
care va sti ca a fost abonat/dezabonat de la topicurile respective.

Dupa care, sunt trimise din coada unul cate unul mesajele catre abonati, iterand
prin tabela cu clienti conectati. La fiecare, se verifica daca topicul mesajului
corespunde cu topicurile la care clientul este abonat (pentru aceasta se fac
copii la aceste siruri de caractere, ce sunt tokenizate dupa `/`) si daca da
se trimite catre acesta, intai un byte ce semnalizeaza ca este un mesaj de primit
(ci nu o confirmare a abonarii/dezabonarii), dupa care se trimite mesajul cu
headere catre client.

La final, sunt inchise toate socket-urile.

## Functionarea clientului

Clientul incepe prin verificarea validitatii parametrilor din linia de comanda,
dupa care is deschide un socket TCP cu care se conecteaza la server. Imediat
dupa, is trimite ID-ul. Dupa care, impreuna cu `stdin`, socket-ul este adaugat
la poll.

Apoi, la fiecare iteratie, clientul face poll si verifica daca a fost introdusa
o comanda. La comanda `exit`, pur si simplu se inchide. La comenzile de
`subscribe` sau `unsubscribe` sunt trimise pachete corespunzatoare catre
server care indica aceasta intentie, folosind structura `subscribe_request`
ce include operatia de efectuat si topicul la care se doreste (dez)abonarea.

Dupa care, se verifica daca serverul s-a deconat, caz in care clientul se
inchide imediat. In cazul in care sunt date de receptionat, intai se
receptioneaza un byte, ce determina daca s-a primit un mesaj sau o
confirmare a unei (dez)abonari. Daca se primeste un mesaj, acesta este
afisat conform cerintei. Iar daca se primeste o confirmare, din nou,
se afiseaza conform cerintei.

La final, este inchis singurul socket deschis.

## Fisierele `utils.hpp` si `utils.cpp`

In cadrul acestora am definit functia`DIE`, pentru a putea verifica usor
si citet rezultatul apelurilor de sistem si a afisa un mesaj si opri programele
in caz de eroare.

Dupa care am adaugat functiile `sendall` si `recvall` pentru a trimite/primi
pachete TCP in intregime (inclusiv daca acestea sunt fragmentate in mai multe bucati).
Apoi am definit structurile `message`, `message_with_header` si `subcribe_request`,
structuri ce transmise intre client si server, ce au `__attribute__((packed))` pentru
a face sigura trimiterea lor pe retea. 

Iar in fisierul `utils.cpp` am implementarea acestor functii, inclusiv a unor
functii menite sa ajute la afisarea pachetelor primite de clientii TCP asa cum
a fost specificat in cerinta.

