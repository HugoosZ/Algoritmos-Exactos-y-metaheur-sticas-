#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <string>
#include <cmath>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

/*
Contexto del ejercicio en T1_AEM_2026.pdf
*/

struct Avion {
    int E, L, P;         // Tiempo Temprano, Tardio, Preferente. (Ek ≤ Pk ≤ Lk); k = 1 ... D; 
    double C_i, C_k;        // Costos. C_i bajo el tiempo preferente. C_k Sobre el tiempo preferente.
};

int D = 0;                        // Cantidad de aviones
int numPistas = 1;                // Numero de pistas (1, 2 o 3)

vector<Avion> aviones;            // Datos de cada avion
vector<vector<int>> tau;          // Matriz de separacion minima. tau[i][j]

vector<int> T_aviones;            // Tiempo asignado a cada avion en solucion parcial
vector<int> pistaActual;          // Pista asignada a cada avion en solucion parcial

vector<int> mejorT;               // Mejor solucion encontrada (tiempos)
vector<int> mejorPista;           // Mejor solucion encontrada (pistas)
double mejorCosto;                // Costo de la mejor solucion

long long nodosExplorados = 0;    // Contador de nodos visitados

vector<vector<int>> dominios;

// Funcion de TIMEOUT
double tiempoLimiteSeg = 60.0;
chrono::steady_clock::time_point inicioEjec;
bool tiempoAgotado = false;

// Revisa si se agoto el tiempo.
inline bool verificar_tiempo() {
    if (tiempoAgotado) return true;
    if ((nodosExplorados & 0xFFF) != 0) return false;
    double s = chrono::duration<double>(chrono::steady_clock::now() - inicioEjec).count();
    if (s > tiempoLimiteSeg) { tiempoAgotado = true; return true; }
    return false;
}

void imprimir_solucion_actual(const string& titulo) {
    cout << titulo << endl;

    if (mejorCosto >= 1e18) {
        cout << "No se encontro una solucion valida." << endl;
        return;
    }

    cout << "Costo: " << mejorCosto << endl;
    cout << "Aviones asignados:" << endl;
    for (int i = 0; i < D; i++) {
        if (mejorT[i] == -1) continue;
        cout << "  Avion " << i << " -> T=" << mejorT[i] << ", Pista=" << mejorPista[i] << endl;
    }
        cout << endl;
}

void imprimir_resumen_ejecucion(const string& nombreAlgoritmo, double costo, long long nodos, double ms) {
    cout << nombreAlgoritmo << ": ";

    if (costo >= 1e18) {
        cout << "sin solucion";
    } else {
        cout << costo;
    }

    cout << " (Nodos: " << nodos << ", Tiempo: " << ms << " ms";

    if (tiempoAgotado) {
        cout << ", TIMEOUT";
    }

    cout << ")" << endl;

    if (tiempoAgotado) {
        imprimir_solucion_actual("Mejor solucion encontrada antes del timeout");
    } else {
        imprimir_solucion_actual("Solucion final sin timeout:");
    }
}

void guardar_resultados_csv(const string& nombreArchivoTest, int pistas, const string& algoritmo,
                            const vector<int>& tiempos, const vector<int>& pistasAsignadas,
                            double costo, long long nodos, double ms) {
    // crear carpeta results/<testname>/<pistas>pistas
    fs::path p(nombreArchivoTest);
    string testbase = p.stem().string();
    fs::path dir = fs::path("results") / testbase / (to_string(pistas) + string("pistas"));
    std::error_code ec;
    fs::create_directories(dir, ec);

    // Archivo de asignaciones
    fs::path assignFile = dir / (algoritmo + string("_assignments.csv"));
    ofstream fa(assignFile);
    if (fa.is_open()) {
        fa << "avion,T,pista\n";
        for (size_t i = 0; i < tiempos.size(); i++) {
            int t = tiempos[i];
            int p_actual = pistasAsignadas[i];
            if (t == -1) fa << i << ",," << (p_actual == -1 ? "" : to_string(p_actual)) << "\n";
            else fa << i << "," << t << "," << (p_actual == -1 ? "" : to_string(p_actual)) << "\n";
        }
        fa.close();
    }

    // Archivo resumen
    fs::path summaryFile = dir / (algoritmo + string("_summary.csv"));
    ofstream fsu(summaryFile);
    if (fsu.is_open()) {
        fsu << "costo,nodos,tiempo_ms,timeout\n";
        fsu << costo << "," << nodos << "," << ms << "," << (tiempoAgotado ? "1" : "0") << "\n";
        fsu.close();
    }
}

/* === Funciones === */

// Verifica la restriccion de separacion entre aviones en la misma pista
// Retorna true si asignar el avion 'avionIdx' al tiempo 'T' en 'pista' es valido
bool restriccion_tiempo(int avionIdx, int T, int pista, const vector<int>& orden, int nivel) {
    // Verificar ventana de tiempo [E, L]
    if (T < aviones[avionIdx].E || T > aviones[avionIdx].L)
        return false;

    // Verificar separacion con todos los aviones ya asignados en la MISMA pista
    for (int k = 0; k < nivel; k++) {
        int otro = orden[k];
        if (pistaActual[otro] != pista) continue; // distinta pista, no hay restriccion

        int T_otro = T_aviones[otro];

        if (T_otro <= T) {
            // 'otro' aterriza antes -> T >= T_otro + tau[otro][avionIdx]
            if (T < T_otro + tau[otro][avionIdx])
                return false;
        } else {
            // avionIdx aterriza antes -> T_otro >= T + tau[avionIdx][otro]
            if (T_otro < T + tau[avionIdx][otro])
                return false;
        }
    }
    return true;
}

// Calcula el costo de un avion dado su tiempo de aterrizaje
double costo_avion(int idx, int T) {
    if (T < aviones[idx].P)
        return (aviones[idx].P - T) * aviones[idx].C_i;
    else
        return (T - aviones[idx].P) * aviones[idx].C_k;
}

// Ordena los aviones por tiempo preferente (P) ascendente
// Los que deben aterrizar antes se asignan primero.
vector<int> ordenar_por_preferente() {
    vector<int> orden(D);
    for (int i = 0; i < D; i++) orden[i] = i;
    sort(orden.begin(), orden.end(), [](int a, int b) {
        return aviones[a].P < aviones[b].P;
    });
    return orden;
}

// Genera tiempos a probar ordenados por cercania al preferente
// Primero P, luego P-1, P+1, P-2, P+2, ...
vector<int> generar_tiempos(int idx) {
    vector<int> tiempos;
    int P = aviones[idx].P;
    int E = aviones[idx].E;
    int L = aviones[idx].L;

    tiempos.push_back(P);
    int maxDelta = max(P - E, L - P);
    for (int d = 1; d <= maxDelta; d++) {
        if (P - d >= E) tiempos.push_back(P - d);
        if (P + d <= L) tiempos.push_back(P + d);
    }
    return tiempos;
}

void backtracking(int nivel, const vector<int>& orden, double costoAcumulado) {
    if (verificar_tiempo()) return;
    nodosExplorados++;
    
    string indent(nivel * 2, ' ');

    // Caso base: todos los aviones fueron asignados
    if (nivel == D) {
        if (costoAcumulado < mejorCosto) {
            mejorCosto = costoAcumulado;
            mejorT = T_aviones;
            mejorPista = pistaActual;
        }
        return;
    }

    int avionIdx = orden[nivel]; // Siguiente avion a asignar (segun heuristica)

    // Generar tiempos candidatos (heuristica de valores)
    vector<int> tiempos = generar_tiempos(avionIdx);

    // Probar cada combinacion de pista y tiempo
    for (int pista = 0; pista < numPistas; pista++) {
        for (int t : tiempos) {

            // Poda: si el costo parcial ya supera el mejor conocido, no seguir
            double costoNuevo = costoAcumulado + costo_avion(avionIdx, t);
            if (costoNuevo >= mejorCosto)
                continue;

            // Verificar restricciones de separacion
            if (restriccion_tiempo(avionIdx, t, pista, orden, nivel)) {
                // Asignar tiempo y pista
                T_aviones[avionIdx] = t;
                pistaActual[avionIdx] = pista;

                // Llamada recursiva al siguiente nivel
                backtracking(nivel + 1, orden, costoNuevo);

                // Deshacer asignacion (backtrack)
                T_aviones[avionIdx] = -1;
                pistaActual[avionIdx] = -1;
            }
        }
    }

}


bool restriccion_tiempo_fc(int avionIdx, int T, int pista) {
    if (T < aviones[avionIdx].E || T > aviones[avionIdx].L)
        return false;

    // Comparar contra TODOS los ya asignados
    for (int otro = 0; otro < D; otro++) {
        if (T_aviones[otro] == -1) continue;
        if (pistaActual[otro] != pista) continue;

        int T_otro = T_aviones[otro];

        if (T_otro <= T) {
            if (T < T_otro + tau[otro][avionIdx])
                return false;
        } else {
            if (T_otro < T + tau[avionIdx][otro])
                return false;
        }
    }

    return true;
}

int seleccionarMRV() {
        int minDomainSize = 1e9;
        int selectedVar = -1;
    
        for (int i = 0; i < D; i++) {
            if (T_aviones[i] != -1) continue; // Ya asignado
    
            int domainSize = dominios[i].size();
            if (domainSize < minDomainSize) {
                minDomainSize = domainSize;
                selectedVar = i;
            }
        }
        return selectedVar;
}


 bool verificar_dos_aviones(int a1, int t1, int p1, int a2, int t2, int p2) {
    if (p1 != p2) return true; // Diferente pista, siempre OK
    
    if (t1 <= t2) {
        return (t2 >= t1 + tau[a1][a2]);
    } else {
        return (t1 >= t2 + tau[a2][a1]);
    }
}

void forward_checking(double costoAcumulado) {
    if (verificar_tiempo()) return;
    nodosExplorados++;

    // Caso base: todos asignados
    bool completo = true;
    for (int i = 0; i < D; i++) {
        if (T_aviones[i] == -1) {
            completo = false;
            break;
        }
    }

    if (completo) {
        if (costoAcumulado < mejorCosto) {
            mejorCosto = costoAcumulado;
            mejorT = T_aviones;
            mejorPista = pistaActual;
        }
        return;
    }

    // MRV
    int avionIdx = seleccionarMRV();
    if (avionIdx == -1) return;

    vector<int> tiempos = dominios[avionIdx];

        int P = aviones[avionIdx].P;

    sort(tiempos.begin(), tiempos.end(),
        [&](int a, int b) {
            int da = abs(a - P);
            int db = abs(b - P);

            if (da != db) return da < db;
            return a < b; // desempate 
        });

    // probar cada combinacion de pista y tiempo
    for (int pista = 0; pista < numPistas; pista++) {

        for (int t : tiempos) {

            double costoNuevo = costoAcumulado + costo_avion(avionIdx, t);
            if (costoNuevo >= mejorCosto) continue;

            // verificar consistencia con los aviones asignados
            if (!restriccion_tiempo_fc(avionIdx, t, pista)) continue;

            auto dominios_backup = dominios;

            T_aviones[avionIdx] = t;
            pistaActual[avionIdx] = pista;

            bool rama_viable = true;
            // Solo filtramos los aviones j que NO están asignados
            for (int j = 0; j < D; j++) {
                if (T_aviones[j] != -1) continue; 

                vector<int> nuevoDominio;
                for (int t_j : dominios[j]) {
                    bool es_posible_en_alguna_pista = false;
                    
                    for (int p_j = 0; p_j < numPistas; p_j++) {
                        // SOLO verificamos contra el avión que acabamos de asignar (avionIdx)
                        // Esto es lo que define al Forward Checking "puro"
                        if (verificar_dos_aviones(avionIdx, t, pista, j, t_j, p_j)) {
                            es_posible_en_alguna_pista = true;
                            break;
                        }
                    }
                    
                    if (es_posible_en_alguna_pista) {
                        nuevoDominio.push_back(t_j);
                    }
                }

                dominios[j] = nuevoDominio;
                if (dominios[j].empty()) {
                    rama_viable = false;
                    break;
                }
            }

            // recursión
            if (rama_viable) {
                forward_checking(costoNuevo);
            }
            // backtrack
            dominios = dominios_backup;
            T_aviones[avionIdx] = -1;
            pistaActual[avionIdx] = -1;
        }
    }
}



bool existe_al_menos_un_tiempo_valido(int idFuturo) {
    for (int p_f = 0; p_f < numPistas; p_f++) {
        for (int t_f = aviones[idFuturo].E; t_f <= aviones[idFuturo].L; t_f++) {
            bool es_posible = true;
            // Comparacion contra TODOS los ya asignados
            for (int otro = 0; otro < D; otro++) {
                if (T_aviones[otro] == -1) continue;  // No asignado aún
                if (pistaActual[otro] != p_f) continue;
                
                int t_a = T_aviones[otro];
                int sep = (t_a <= t_f) ? tau[otro][idFuturo] : tau[idFuturo][otro];
                
                if (abs(t_f - t_a) < sep) {
                    es_posible = false;
                    break;
                }
            }
            if (es_posible) return true;
        }
    }
    return false;
}

void minimal_forward_checking(int nivel, const vector<int>& orden, double costoAcumulado) {
    if (verificar_tiempo()) return;
    nodosExplorados++;

    // Caso base
    if (nivel == D) {
        if (costoAcumulado < mejorCosto) {
            mejorCosto = costoAcumulado;
            mejorT = T_aviones;
            mejorPista = pistaActual;
        }
        return;
    }

    int avionIdx = orden[nivel]; // Siguiente avion a asignar (segun heuristica)

    // Generar tiempos candidatos (heuristica de valores)
    vector<int> tiempos = generar_tiempos(avionIdx);

    // Probar cada combinacion de pista y tiempo
    for (int pista = 0; pista < numPistas; pista++) {
        for (int t : tiempos) {

            // Poda: si el costo parcial ya supera el mejor conocido, no seguir
            double costoNuevo = costoAcumulado + costo_avion(avionIdx, t);
            if (costoNuevo >= mejorCosto)
                continue;

            // Verificar restricciones de separacion
            if (!restriccion_tiempo(avionIdx, t, pista, orden, nivel)) {
                continue; // Si no es valido, ni siquiera asignamos, vamos al siguiente candidato 
            }

            // --- INICIO DEL FORWARD CHECKING ---

            // Paso 1: Asignación temporal
            T_aviones[avionIdx] = t;
            pistaActual[avionIdx] = pista;

            // Paso 2: Mirar hacia el futuro
            bool futuro_viable = true;
            for (int k = nivel + 1; k < D; k++) {
                // Si un solo avión futuro se queda sin opciones, se poda la rama
                if (!existe_al_menos_un_tiempo_valido(orden[k])) {
                    futuro_viable = false; 
                    break;
                }
            }

            // Paso 3. RECURSIÓN: Solo si el futuro es viable
            if (futuro_viable) {
                minimal_forward_checking(nivel + 1, orden, costoNuevo);
            }

            // Deshacer asignacion (backtrack)
            T_aviones[avionIdx] = -1;
            pistaActual[avionIdx] = -1;
        }
    }
}

bool leer_archivo(const string& nombre) {
    ifstream archivo(nombre);
    if (!archivo.is_open()) {
        cerr << "Error: no se pudo abrir " << nombre << endl;
        return false;
    }

    archivo >> D;
    aviones.resize(D);
    tau.resize(D, vector<int>(D, 0));

    for (int i = 0; i < D; i++) {
        archivo >> aviones[i].E >> aviones[i].P >> aviones[i].L;
        archivo >> aviones[i].C_i >> aviones[i].C_k;

        // Leer fila de separacion del avion i con todos los demas
        for (int j = 0; j < D; j++) {
            archivo >> tau[i][j];
        }
    }

    archivo.close();
    return true;
}


int main(int argc, char* argv[]) {
    cout << "Control Aereo" << endl;

    string nombreArchivo = "case1.txt"; // Probar con case1.txt por el momento
    numPistas = 1;

    // Argumentos: ./programa <archivo> <num_pistas>
    if (argc >= 2) nombreArchivo = argv[1];
    if (argc >= 3) {
        numPistas = atoi(argv[2]);
        if (numPistas < 1 || numPistas > 3) {
            cerr << "Error: pistas debe ser 1, 2 o 3" << endl;
            return 1;
        }
    }

    cout << "Archivo: " << nombreArchivo << endl;
    cout << "Pistas:  " << numPistas << endl;

    // Leer datos del archivo
    if (!leer_archivo(nombreArchivo)) return 1;
    cout << "Aviones: " << D << endl << endl;

    // Inicializar estructuras
    T_aviones.assign(D, -1);
    pistaActual.assign(D, -1);
    mejorT.assign(D, -1);
    mejorPista.assign(D, -1);

    // Heuristica de variables: ordenar por tiempo preferente
    vector<int> orden = ordenar_por_preferente();

    double costo_back = 0, costo_forward = 0, costo_minimal_forward = 0; // Costo total de cada algoritmo.

    cout << "Resultados para " << nombreArchivo << " con " << numPistas << " pistas:" << endl;
    // ---  BACKTRACKING ---
    mejorCosto = 1e18; // Resetear récord
    nodosExplorados = 0;
    tiempoAgotado = false;
    inicioEjec = chrono::steady_clock::now();
    auto t0 = chrono::steady_clock::now();
    backtracking(0, orden, 0.0);
    auto t1 = chrono::steady_clock::now();
    costo_back = mejorCosto;
    long long nodos_back = nodosExplorados;
    double ms_back = chrono::duration<double, milli>(t1 - t0).count();
    // Guardar copia de la mejor solucion encontrada por backtracking
    vector<int> bestT_back = mejorT;
    vector<int> bestP_back = mejorPista;
    imprimir_resumen_ejecucion("Costo Backtracking", costo_back, nodos_back, ms_back);
    guardar_resultados_csv(nombreArchivo, numPistas, string("Backtracking"), bestT_back, bestP_back, costo_back, nodos_back, ms_back);

    // --- FORWARD CHECKING ---
    // Inicializar dominios
    dominios.resize(D);

    for (int i = 0; i < D; i++) {
        dominios[i].clear();
        for (int t = aviones[i].E; t <= aviones[i].L; t++) {
            dominios[i].push_back(t);
        }
    }
    
    mejorCosto = 1e18; 
    nodosExplorados = 0;
    tiempoAgotado = false;
    inicioEjec = chrono::steady_clock::now();
    t0 = chrono::steady_clock::now();
    forward_checking(0.0);
    t1 = chrono::steady_clock::now();
    costo_forward = mejorCosto;
    long long nodos_fc = nodosExplorados;
    double ms_fc = chrono::duration<double, milli>(t1 - t0).count();
    vector<int> bestT_fc = mejorT;
    vector<int> bestP_fc = mejorPista;
    imprimir_resumen_ejecucion("Costo FC", costo_forward, nodos_fc, ms_fc);
    guardar_resultados_csv(nombreArchivo, numPistas, string("ForwardChecking"), bestT_fc, bestP_fc, costo_forward, nodos_fc, ms_fc);

    // --- MINIMAL FORWARD CHECKING ---
    mejorCosto = 1e18; 
    nodosExplorados = 0;
    tiempoAgotado = false;
    inicioEjec = chrono::steady_clock::now();
    t0 = chrono::steady_clock::now();
    minimal_forward_checking(0, orden, 0.0);
    t1 = chrono::steady_clock::now();
    costo_minimal_forward = mejorCosto;
    long long nodos_mfc = nodosExplorados;
    double ms_mfc = chrono::duration<double, milli>(t1 - t0).count();    
    vector<int> bestT_mfc = mejorT;
    vector<int> bestP_mfc = mejorPista;
    imprimir_resumen_ejecucion("Costo MFC", costo_minimal_forward, nodos_mfc, ms_mfc);
    guardar_resultados_csv(nombreArchivo, numPistas, string("MinimalForwardChecking"), bestT_mfc, bestP_mfc, costo_minimal_forward, nodos_mfc, ms_mfc);
    
    return 0;
}