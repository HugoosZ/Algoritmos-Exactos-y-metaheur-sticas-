#include <iostream>
#include <vector>

using namespace std;

/*
Contexto del ejercicio en T1_AEM_2026.pdf
*/

struct Avion {
    int E, L, P;         // Tiempo Temprano, Tardio, Preferente. (Ek ≤ Pk ≤ Lk); k = 1 ... D; 
    int C_i, C_k;        // Costos. C_i bajo el tiempo preferente. C_k Sobre el tiempo preferente.
};

vector<Avion> aviones;   // Vector de aviones. Cada avion tiene su tiempo preferente, tardio, preferente y sus costos.
vector<int> T_aviones;   // Vector de tiempos asignados a cada avion. T_aviones[i] = T_i; i = 1 ... D;
vector<vector<int>> tau; // Matriz de tiempos mínimos entre aviones. tau[i][j] = t_ij; i, j = 1 ... D; i != j; tau[i][i] = 0;

vector<int> pistaActual; // pista asignada a cada avion

bool restriccion_tiempo(int T){
    
    return true; // Verificar que se cumplan las restricciones de tiempo entre aviones. (T_i < T_j); (T_j ≥ T_i + t_ij )
}

int costo_avion(int T, int E_k, int P_k, int L_k, int C_i, int C_k) { // Funcion a minimizar.
    if (T < P_k) return (P_k - T) * C_i;
    if (T > P_k) return (T - P_k) * C_k;
    else return 0;
}
int backtracking() {
    // En cada funcion integrar la funcion de costo_avion y la restriccion_tiempo para verificar que se cumplan las restricciones de tiempo entre aviones.
    /*
    T_aviones.push_back(T); // Tiempo asignado al primer avion. (T_1)
    if(T_aviones.size() < 2 && T_aviones.size() > 0){
        

    

        
    } else if (T_aviones.size() > 1){
        est = restriccion_tiempo(T);
        if(est == false){
            cout << "No se cumple la restriccion de tiempo entre aviones" << endl;
            return 0;
        } else {
            for (int i = 0; i < T_aviones.size(); i++){
                costo += costo_avion(T_aviones[i], aviones[i].E, aviones[i].P, aviones[i].L, aviones[i].C_i, aviones[i].C_k);
            }
        }
    }
    */
    return 0; 
}
int forward_checking() {
    return 0;

}
int minimal_forward_checking() {
    return 0;

}



int main() {
    cout << "Control Aereo" << endl;

    int D = 0; // Cantidad de aviones
    int T = 0; // Tiempo asignado. Si el avion i aterriza en el tiempo T_i, El avion j deberá aterrizar en T_j. (T_i < T_j); (T_j ≥ T_i + t_ij )

    int costo_back, costo_forward, costo_minimal_forward = 0; // Costo total de cada algoritmo.

    int numPistas = 1; // Numero de pistas (Default = 1)

    costo_back = backtracking();
    costo_forward = forward_checking();
    costo_minimal_forward = minimal_forward_checking();


    cout << "Costo Backtracking: " << costo_back << endl;
    cout << "Costo Forward Checking: " << costo_forward << endl;
    cout << "Costo Minimal Forward Checking: " << costo_minimal_forward << endl;
    
    return 0;
}