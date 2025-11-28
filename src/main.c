#include <stdio.h>
#include <string.h>
#include "bigint.h"
#include "ec.h"
#include "sha256.h"
#include "aes.h"

int main() {
    printf("=== ECIES Full Encryption Demo ===\n");

    ec_point_t G;
    ec_init_g(&G);

    // --- SETUP: BOB'S STATIC KEY ---
    bigint256_t bob_priv;
    bigint_set_hex(&bob_priv, "B0B5ECA123456789B0B5ECA123456789B0B5ECA123456789B0B5ECA123456789"); 
    ec_point_t bob_pub;
    ec_mul(&bob_pub, &bob_priv, &G);
    printf("[Setup] Bob's Public Key is ready.\n");

    // ALICE: SENDER
    char *secret_msg = "Hello Bob! This is ECIES from scratch.";
    printf("\n[Alice] Message to send: \"%s\"\n", secret_msg);

    // 1. Generate Ephemeral Key
    bigint256_t alice_priv;
    bigint_set_hex(&alice_priv, "A11CECA123456789A11CECA123456789A11CECA123456789A11CECA123456789");
    ec_point_t R; // Public Ephemeral Key
    ec_mul(&R, &alice_priv, &G);

    // 2. Derive Shared Secret S
    ec_point_t S_alice;
    ec_mul(&S_alice, &alice_priv, &bob_pub);

    // 3. Derive AES Key (Hash S.x)
    uint8_t shared_bytes[32];
    for(int i=0; i<4; i++) {
        uint64_t limb = S_alice.x.limbs[3-i];
        for(int j=0; j<8; j++) shared_bytes[i*8 + j] = (limb >> (56 - j*8)) & 0xFF;
    }
    uint8_t aes_key[32];
    sha256_ctx_t sha;
    sha256_init(&sha);
    sha256_update(&sha, shared_bytes, 32);
    sha256_final(&sha, aes_key);

    // 4. Encrypt using AES-128-CTR
    // Use the first 16 bytes of the hash for the AES-128 key
    aes_ctx_t aes_alice;
    aes_init(&aes_alice, aes_key); 

    // Prepare Nonce (random 12 bytes, usually)
    uint8_t nonce[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    
    // Encrypt
    uint8_t ciphertext[100];
    int msg_len = strlen(secret_msg);
    memcpy(ciphertext, secret_msg, msg_len);
    
    aes_ctr_encrypt(&aes_alice, nonce, ciphertext, msg_len);

    printf("[Alice] Encrypted Ciphertext: ");
    for(int i=0; i<msg_len; i++) printf("%02x", ciphertext[i]);
    printf("\n");

    // BOB: RECEIVER
    printf("\n--- Transmitting (R, Ciphertext) to Bob ---\n");

    // 1. Bob receives R. Derives S.
    ec_point_t S_bob;
    ec_mul(&S_bob, &bob_priv, &R);

    // 2. Derive AES Key
    uint8_t shared_bytes_bob[32];
    for(int i=0; i<4; i++) {
        uint64_t limb = S_bob.x.limbs[3-i];
        for(int j=0; j<8; j++) shared_bytes_bob[i*8 + j] = (limb >> (56 - j*8)) & 0xFF;
    }
    uint8_t aes_key_bob[32];
    sha256_init(&sha);
    sha256_update(&sha, shared_bytes_bob, 32);
    sha256_final(&sha, aes_key_bob);

    // 3. Decrypt
    // AES-CTR decryption is identical to encryption (XOR again)
    aes_ctx_t aes_bob;
    aes_init(&aes_bob, aes_key_bob);
    
    uint8_t decrypted[100];
    memcpy(decrypted, ciphertext, msg_len);
    decrypted[msg_len] = '\0'; // Null terminate for printing

    aes_ctr_encrypt(&aes_bob, nonce, decrypted, msg_len);

    printf("[Bob] Decrypted Message: \"%s\"\n", decrypted);

    return 0;
}