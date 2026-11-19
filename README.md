[![CI](https://github.com/Alexandru296/map-proiect/actions/workflows/ci.yml/badge.svg)](https://github.com/Alexandru296/map-proiect/actions/workflows/ci.yml)
# Agenda de contacte

Proiect individual la disciplina Metode avansate de programare, anul universitar 2026-2027.

## Autor

- **Nume:** Costea Alexandru-Mihai
- **Grupa:** 1.1
- **Marca:** 715678
- **Tema:** 1 - agenda de contacte

## Descriere

Serviciu web pentru o agenda de contacte: adaugare, listare cu filtrare dupa categorie si cautare dupa nume, stergere si statistici pe categorii.

## Tehnologii

 C++20 cu cpp-httplib si nlohmann/json

## Rulare

```
docker build -t map-proiect .
docker run -d -p 8080:8080 map-proiect
```

Aplicatia asculta pe portul 8080. Verificati:

```
curl http://localhost:8080/health
curl http://localhost:8080/version
```

## Testare

```
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j
./build/tests
```

## Rutele implementate

| Ruta | Metoda | Descriere |
|---|---|---|
| `/health` | GET | Starea serviciului |
| `/version` | GET | Versiunea si commit-ul din care a fost construita imaginea |
| `/` | GET | Pagina de prezentare |
| `/reset` | POST | Goleste datele din memorie |
| `/contacts` | POST | Adauga un contact; 201 si contactul creat, cu id |
| `/contacts` | GET | Listeaza contactele, filtrate dupa `category` si `q`, ordonate dupa nume |
| `/contacts/{id}` | GET | Intoarce un contact sau 404 |
| `/contacts/{id}` | DELETE | Sterge un contact; 204 sau 404 |
| `/stats` | GET | Numarul total de contacte si numarul pe fiecare categorie |

## Decizii de implementare

- **Emailul se compara fara majuscule, dar se pastreaza cum a fost scris.** La verificarea de duplicat ambele adrese sunt trecute prin `ToLower`, pentru ca `Ana@Example.com` si `ana@example.com` sunt aceeasi adresa. In raspuns se intoarce forma scrisa de utilizator.

- **Logica sta in `app.hpp`, separat de server.** Clasa `Store` (adaugare, cautare, filtrare, stergere, statistici) nu depinde de HTTP, deci poate fi testata direct cu doctest, fara sa pornesc serverul. `main.cpp` doar leaga rutele de aceste functii.

- **Contactele sunt pastrate intr-un `std::vector`, protejat de un `std::mutex`.** Serverul poate trata mai multe cereri in paralel, iar mutex-ul impiedica doua cereri sa modifice lista in acelasi timp.
