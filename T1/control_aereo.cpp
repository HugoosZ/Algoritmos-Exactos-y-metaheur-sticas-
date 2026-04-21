#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <string>
#include <cmath>

using namespace std;

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
    nodosExplorados++;
    
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

int forward_checking() {
    return 0;

}

bool existe_al_menos_un_tiempo_valido(int idFuturo, int nivel_actual, const vector<int>& orden) {
    // Revisa todas las pistas posibles
    for (int p_futura = 0; p_futura < numPistas; p_futura++) {
        // Y todos los tiempos posibles
        for (int t_futuro = aviones[idFuturo].E; t_futuro <= aviones[idFuturo].L; t_futuro++) {
            // Si encuentra UNA sola combinación que cumpla las restricciones, el avión futuro es viable
            if (restriccion_tiempo(idFuturo, t_futuro, p_futura, orden, nivel_actual + 1)) {
                return true; 
            }
        }
    }
    return false; // Si recorrió todo y no encontró nada, el futuro es inviable
}

void minimal_forward_checking(int nivel, const vector<int>& orden, double costoAcumulado) {
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
                int idFuturo = orden[k];
                bool existe_opcion = false;

                // ¿Tiene alguna opción el avión k?
                if (!existe_al_menos_un_tiempo_valido(idFuturo, nivel, orden)) {
                    futuro_viable = false; // Si uno solo no tiene opciones, esta rama muere
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
    backtracking(0, orden, 0.0);
    costo_back = mejorCosto;
    long long nodos_back = nodosExplorados;
    cout << "Costo Backtracking: " << costo_back << " (Nodos: " << nodos_back << ")" << endl;


    // --- MINIMAL FORWARD CHECKING ---
    mejorCosto = 1e18; // Resetear récord para que MFC busque por su cuenta
    nodosExplorados = 0;
    minimal_forward_checking(0, orden, 0.0);
    costo_minimal_forward = mejorCosto;
    long long nodos_mfc = nodosExplorados;


    
    cout << "Costo MFC:          " << costo_minimal_forward << " (Nodos: " << nodos_mfc << ")" << endl;
    
    return 0;
}