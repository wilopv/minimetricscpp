minimetricscpp
==============

Servidor HTTP simple en C++ utilizando la libreria Boost, que expone las metricas de cpu y memoria de /proc de los sistemas Linux en un endpoint /metrics, en un formato disponible para importar en Prometheus para posterior analisis.


Instrucciones de compilacion y uso
-----
1. `cmake -S . -B build`
2. `cmake --build build`
3. `./build/minimetrics`
4. Visitar `http://localhost:8080/metrics` o `http://localhost:8080/healthz`, 
(O tambien utilizando el comando curl en terminal)

