#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>

// -----------------------------------------------------------------------------
// TABLAS CONSTANTES DE PERMUTACIÓN Y CORRIMIENTO (Estándar DES)
// -----------------------------------------------------------------------------

// Tabla PC-1: Mapea 64 bits de entrada a 56 bits (descarta bits de paridad)
static const int PC1[56] = {
    57, 49, 41, 33, 25, 17,  9,
     1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,
    19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
     7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,
    21, 13,  5, 28, 20, 12,  4
};

// Cantidad de posiciones a desplazar hacia la izquierda por cada ronda (1 a 16)
static const int DESPLAZAMIENTOS[16] = {
    1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1
};

// Tabla PC-2 (Caja P): Permutación con compresión de 56 bits a 48 bits
static const int PC2[48] = {
    14, 17, 11, 24,  1,  5,
     3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,
    16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

// -----------------------------------------------------------------------------
// FUNCIONES AUXILIARES Y MANEJO DE BITS
// -----------------------------------------------------------------------------

uint64_t get_rand_64() {

    uint64_t high = (uint64_t)rand();

    uint64_t low = (uint64_t)rand();

    return (high << 32) | low;

}

/**
 * Lee un archivo por bloques de 64 bits (8 bytes) y genera una lista de llaves.
 * @param filename Ruta del archivo a leer.
 * @param out_count Puntero para retornar la cantidad de llaves generadas.
 * @return Un arreglo dinámico de uint64_t con las llaves. Recuerda liberarlo con free() luego.
 */
uint64_t* generar_llaves_desde_archivo(const char *filename, size_t *out_count) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error al abrir el archivo");
        *out_count = 0;
        return NULL;
    }

    size_t capacidad = 10;
    size_t cantidad = 0;
    uint64_t *llaves = malloc(capacidad * sizeof(uint64_t));
    if (!llaves) {
        fclose(file);
        *out_count = 0;
        return NULL;
    }

    unsigned char buffer[8]; // 8 bytes = 64 bits
    size_t bytes_leidos;

    // Leer el archivo de 8 en 8 bytes (cada 64 bits)
    while ((bytes_leidos = fread(buffer, 1, 8, file)) > 0) {

        uint64_t nueva_llave = get_rand_64();

        // Redimensionar el arreglo si nos quedamos sin espacio
        if (cantidad >= capacidad) {
            capacidad *= 2;
            uint64_t *temp = realloc(llaves, capacidad * sizeof(uint64_t));
            if (!temp) {
                perror("Error al reasignar memoria");
                free(llaves);
                fclose(file);
                *out_count = 0;
                return NULL;
            }
            llaves = temp;
        }

        llaves[cantidad++] = nueva_llave;
    }

    fclose(file);
    *out_count = cantidad;
    return llaves;
}

// Convierte un valor entero a una representación en string binario de N bits
void bits_a_string(uint64_t valor, int num_bits, char *buffer) {
    for (int i = 0; i < num_bits; i++) {
        buffer[i] = ((valor >> (num_bits - 1 - i)) & 1ULL) ? '1' : '0';
    }
    buffer[num_bits] = '\0';
}

// Aplica la permutación PC-1 (convierte 64 bits a 56 bits)
uint64_t aplicar_pc1(uint64_t clave64) {
    uint64_t resultado56 = 0;
    for (int i = 0; i < 56; i++) {
        int pos_origen = PC1[i]; // Índice de 1 a 64 (MSB a LSB)
        uint64_t bit = (clave64 >> (64 - pos_origen)) & 1ULL;
        resultado56 |= (bit << (55 - i));
    }
    return resultado56;
}

// Realiza la rotación circular a la izquierda acotada estrictamente a 28 bits
uint32_t rotar_izquierda_28(uint32_t bloque28, int shifts) {
    return ((bloque28 << shifts) | (bloque28 >> (28 - shifts))) & 0x0FFFFFFF;
}

// Aplica la permutación/compresión PC-2 (Caja P: convierte 56 bits a 48 bits)
uint64_t aplicar_pc2(uint64_t bloque56) {
    uint64_t subclave48 = 0;
    for (int i = 0; i < 48; i++) {
        int pos_origen = PC2[i]; // Índice de 1 a 56 (MSB a LSB)
        uint64_t bit = (bloque56 >> (56 - pos_origen)) & 1ULL;
        subclave48 |= (bit << (47 - i));
    }
    return subclave48;
}

// Imprime simultáneamente en consola y en el archivo de bitácora
void log_mensaje(FILE *archivo, const char *formato, ...) {
    va_list args;
    
    // Imprimir en consola
    va_start(args, formato);
    vprintf(formato, args);
    va_end(args);

    // Imprimir en archivo de bitácora
    if (archivo != NULL) {
        va_start(args, formato);
        vfprintf(archivo, formato, args);
        va_end(args);
    }
}

// -----------------------------------------------------------------------------
// PROGRAMA PRINCIPAL
// -----------------------------------------------------------------------------
int main(void) {

    srand(time(NULL));

    // Abrir archivo de bitácora para registro
    FILE *bitacora = fopen("bitacora_subclaves.log", "w");
    if (bitacora == NULL) {
        perror("Error al crear el archivo de bitácora");
        return EXIT_FAILURE;
    }

    // // Clave de entrada de 64 bits (Ejemplo hexadecimal de prueba)
    // uint64_t clave_original = 0x133457799BBCDFF1ULL;

    char buf_bin64[65], buf_bin56[57], buf_bin28_c[29], buf_bin28_d[29], buf_bin48[49];


    log_mensaje(bitacora, "=========================================================================\n");
    log_mensaje(bitacora, "           SIMULACION DE GENERACION DE SUBCLAVES DERIVADAS (DES)         \n");
    log_mensaje(bitacora, "=========================================================================\n\n");


    size_t total_llaves = 0;
    // Cambia "archivo_prueba.txt" por la ruta real de tu archivo
    uint64_t *lista_llaves = generar_llaves_desde_archivo("prueba.txt", &total_llaves);

    if (lista_llaves) {
        log_mensaje(bitacora, "Se procesó el archivo exitosamente.\n");
        log_mensaje(bitacora, "Total de bloques de 64 bits leídos -> Total de llaves generadas: %zu\n\n", total_llaves);
    }
    
    for (size_t i = 0; i < total_llaves; i++) {
        uint64_t clave_original = lista_llaves[i];
        log_mensaje(bitacora, "[%zu / %zu] Iniciando generación de subllaves...\n\n", i + 1, total_llaves);
        bits_a_string(clave_original, 64, buf_bin64);
        log_mensaje(bitacora, "Clave Original de Entrada (64 bits): 0x%016llX\n", clave_original);
        log_mensaje(bitacora, "Representación Binaria:              %s\n\n", buf_bin64);
        // printf("Llave %zu: %016llX\n", i + 1, lista_llaves[i]);
        
        // Paso 1: Reducción de 64 a 56 bits descartando bits de paridad mediante PC-1
        uint64_t clave_56 = aplicar_pc1(clave_original);
        bits_a_string(clave_56, 56, buf_bin56);
        
        log_mensaje(bitacora, "[PC-1] Clave reducida a 56 bits (sin paridad): 0x%014llX\n", clave_56);
        log_mensaje(bitacora, "       Binario (56 bits): %s\n\n", buf_bin56);

        // Paso 2: División en 2 bloques de 28 bits (C0 y D0)
        uint32_t C = (uint32_t)((clave_56 >> 28) & 0x0FFFFFFF);
        uint32_t D = (uint32_t)(clave_56 & 0x0FFFFFFF);

        bits_a_string(C, 28, buf_bin28_c);
        bits_a_string(D, 28, buf_bin28_d);
        log_mensaje(bitacora, "Bloques Iniciales de 28 bits:\n");
        log_mensaje(bitacora, " C0: 0x%07X | %s\n", C, buf_bin28_c);
        log_mensaje(bitacora, " D0: 0x%07X | %s\n\n", D, buf_bin28_d);

        log_mensaje(bitacora, "-------------------------------------------------------------------------\n");
        log_mensaje(bitacora, " Rda | Shift |  C_i (28 bits)  |  D_i (28 bits)  | Subclave K_i (48 bits Hex) \n");
        log_mensaje(bitacora, "-------------------------------------------------------------------------\n");

        // Paso 3: Bucle de 16 Rondas
        for (int ronda = 1; ronda <= 16; ronda++) {
            int shift = DESPLAZAMIENTOS[ronda - 1];

            // Aplica rotación circular a la izquierda en cada bloque de 28 bits
            C = rotar_izquierda_28(C, shift);
            D = rotar_izquierda_28(D, shift);

            // Une C_i y D_i en un bloque continuo de 56 bits
            uint64_t CD_combinado = (((uint64_t)C) << 28) | ((uint64_t)D);

            // Compresión/Permutación mediante la Caja P (PC-2) para obtener subclave de 48 bits
            uint64_t subclave_48 = aplicar_pc2(CD_combinado);

            // Formateo para la bitácora
            bits_a_string(C, 28, buf_bin28_c);
            bits_a_string(D, 28, buf_bin28_d);
            bits_a_string(subclave_48, 48, buf_bin48);

            log_mensaje(bitacora, " %2d  |   %d   | 0x%07X       | 0x%07X       | 0x%012llX \n", 
                        ronda, shift, C, D, subclave_48);
            
            // // Log detallado del valor binario de la subclave obtenida
            // log_mensaje(bitacora, "     |       | Bin: %s %s | Subclave Bin: %s\n", 
            //             buf_bin28_c, buf_bin28_d, buf_bin48);
            log_mensaje(bitacora, "-------------------------------------------------------------------------\n\n");
        }
    }
    log_mensaje(bitacora, "\nSimulación completada con éxito. Registro guardado en 'bitacora_subclaves.log'.\n");

    fclose(bitacora);
    
    // Liberar la memoria dinámica reservada
    free(lista_llaves);
    return EXIT_SUCCESS;
}