#include "header.h"

#define KEY_LENGTH 32  // AES-256 key size in bytes
#define IV_LENGTH 16   // AES block size in bytes

/**
 * Encrypts input data using AES-256-CBC.
 *
 * @param plain_text      Input data to encrypt.
 * @param plain_text_len  Length of plain_text.
 * @param key             32-byte encryption key.
 * @param iv              16-byte initialization vector.
 * @param cipher_text     Output buffer for encrypted data.
 * @return                Length of encrypted data on success.
 */
int encrypt(unsigned char *plain_text, int plain_text_len, unsigned char *key, unsigned char *iv, unsigned char *cipher_text) {
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();  // Create new encryption context

    int len, cipher_text_len;

    EVP_EncryptInit_ex(context, EVP_aes_256_cbc(), NULL, key, iv);  // Initialize AES-256-CBC cipher

    EVP_EncryptUpdate(context, cipher_text, &len, plain_text, plain_text_len);  // Encrypt data
    cipher_text_len = len;

    EVP_EncryptFinal_ex(context, cipher_text + len, &len);  // Finalize encryption (handle padding)
    cipher_text_len += len;

    EVP_CIPHER_CTX_free(context);  // Clean up context

    return cipher_text_len;
}

/**
 * Decrypts encrypted data using AES-256-CBC.
 *
 * @param cipher_text      Input encrypted data.
 * @param cipher_text_len  Length of cipher_text.
 * @param key              32-byte decryption key.
 * @param iv               16-byte initialization vector.
 * @param plain_text       Output buffer for decrypted data.
 * @return                 Length of decrypted data on success.
 */
int decrypt(unsigned char *cipher_text, int cipher_text_len, unsigned char *key, unsigned char *iv, unsigned char *plain_text) {
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();  // Create new decryption context

    int len, plain_text_len;

    EVP_DecryptInit_ex(context, EVP_aes_256_cbc(), NULL, key, iv);  // Initialize AES-256-CBC cipher

    EVP_DecryptUpdate(context, plain_text, &len, cipher_text, cipher_text_len);  // Decrypt data
    plain_text_len = len;

    EVP_DecryptFinal_ex(context, plain_text + len, &len);  // Finalize decryption (remove padding)
    plain_text_len += len;

    EVP_CIPHER_CTX_free(context);  // Clean up context

    return plain_text_len;
}

/**
 * Encrypts file contents and sends them over a socket.
 *
 * @param in_file      File pointer to the input file.
 * @param buffer_size  Size of the read buffer.
 * @param socket       Socket file descriptor to send data through.
 * @param key          32-byte encryption key.
 * @param iv           16-byte initialization vector.
 * @return             0 on success, -1 on failure.
 */
int encrypt_file(FILE *in_file, int buffer_size, int socket, unsigned char *key, unsigned char *iv) {
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) return -1;

    if (EVP_EncryptInit_ex(context, EVP_aes_256_cbc(), NULL, key, iv) != 1) return -1;

    unsigned char in_buffer[buffer_size];                           // Buffer for plaintext file chunks
    unsigned char out_buffer[buffer_size + EVP_MAX_BLOCK_LENGTH];   // Buffer for ciphertext output
    int in_len, out_len;

    // Read from file and send encrypted chunks
    while ((in_len = fread(in_buffer, 1, buffer_size, in_file)) > 0) {
        if (EVP_EncryptUpdate(context, out_buffer, &out_len, in_buffer, in_len) != 1) return -1;
        if (send(socket, out_buffer, out_len, 0) < 0) return -1;
    }

    // Final encryption block
    if (EVP_EncryptFinal_ex(context, out_buffer, &out_len) != 1) return -1;
    if (send(socket, out_buffer, out_len, 0) < 0) return -1;

    EVP_CIPHER_CTX_free(context);

    return 0;
}

/**
 * Receives encrypted data from a socket, decrypts it, and writes to a file.
 *
 * @param out_file     File pointer to the output file.
 * @param buffer_size  Size of the read buffer.
 * @param socket       Socket file descriptor to receive data from.
 * @param key          32-byte decryption key.
 * @param iv           16-byte initialization vector.
 * @return             0 on success, -1 on failure.
 */
int decrypt_file(FILE *out_file, int buffer_size, int socket, unsigned char *key, unsigned char *iv) {
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (!context) return -1;

    if (EVP_DecryptInit_ex(context, EVP_aes_256_cbc(), NULL, key, iv) != 1) return -1;

    unsigned char in_buffer[buffer_size + EVP_MAX_BLOCK_LENGTH];   // Buffer for incoming ciphertext
    unsigned char out_buffer[buffer_size];                         // Buffer for decrypted plaintext
    int in_len, out_len;

    // Receive and decrypt data from socket
    while ((in_len = recv(socket, in_buffer, sizeof(in_buffer), 0)) > 0) {
        if (EVP_DecryptUpdate(context, out_buffer, &out_len, in_buffer, in_len) != 1) return -1;

        fwrite(out_buffer, 1, out_len, out_file);
    }

    // Final decryption block
    if (EVP_DecryptFinal_ex(context, out_buffer, &out_len) != 1) return -1;

    fwrite(out_buffer, 1, out_len, out_file);

    EVP_CIPHER_CTX_free(context);

    return 0;
}
