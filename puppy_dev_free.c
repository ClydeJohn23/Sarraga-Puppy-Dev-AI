#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <time.h>
#include <ctype.h>

#define VERSION "2.0.0 FREE"
#define DEVELOPER "John Clyde"
#define AI_NAME "Puppy Dev"
#define MAX_INPUT 4096
#define MAX_RESPONSE 32768

// Color codes
#define PINK "\033[38;5;213m"
#define PURPLE "\033[38;5;141m"
#define BLUE "\033[38;5;117m"
#define GREEN "\033[38;5;120m"
#define YELLOW "\033[38;5;228m"
#define ORANGE "\033[38;5;215m"
#define RED "\033[38;5;210m"
#define CYAN "\033[38;5;159m"
#define WHITE "\033[97m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define RESET "\033[0m"

// Conversation history
typedef struct {
    char role[20];
    char *content;
} Message;

typedef struct {
    char *data;
    size_t size;
} Response;

Message *conversation = NULL;
int message_count = 0;
int conversation_capacity = 0;

// API Configuration - GROQ (FREE!)
char API_KEY[512] = "";
const char *API_URL = "https://api.groq.com/openai/v1/chat/completions";
const char *MODEL = "llama-3.3-70b-versatile"; // Free and powerful!

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if(ptr == NULL) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

void print_banner() {
    printf(PINK);
    printf("\n");
    printf("    ╔══════════════════════════════════════════════════╗\n");
    printf("    ║                                                  ║\n");
    printf("    ║      🐶  " BOLD "PUPPY DEV - AI ASSISTANT" RESET PINK "  🐾          ║\n");
    printf("    ║                                                  ║\n");
    printf("    ╠══════════════════════════════════════════════════╣\n");
    printf("    ║  " CYAN "Version:" PINK " %-38s ║\n", VERSION);
    printf("    ║  " CYAN "Developer:" PINK " %-36s ║\n", DEVELOPER);
    printf("    ║  " CYAN "Powered by:" PINK " Groq AI (100%% FREE!)          ║\n");
    printf("    ╚══════════════════════════════════════════════════╝\n");
    printf(RESET);
    printf("\n");
    printf(BLUE "    🐕 " BOLD "Woof! I'm Puppy Dev, your FREE AI assistant!\n" RESET);
    printf(DIM "    I can help with coding, questions, and conversations.\n" RESET);
    printf("\n");
}

void print_puppy_thinking() {
    const char *frames[] = {
        "🐶 Thinking.",
        "🐶 Thinking..",
        "🐶 Thinking..."
    };
    
    for (int i = 0; i < 3; i++) {
        printf("\r    " YELLOW "%s" RESET, frames[i % 3]);
        fflush(stdout);
        usleep(300000);
    }
    printf("\r                    \r");
}

void setup_api_key() {
    char *env_key = getenv("GROQ_API_KEY");
    
    if (env_key != NULL && strlen(env_key) > 0) {
        strncpy(API_KEY, env_key, sizeof(API_KEY) - 1);
        printf(GREEN "    ✓ API key loaded from environment\n" RESET);
        return;
    }
    
    FILE *config = fopen("puppy_config.txt", "r");
    if (config != NULL) {
        if (fgets(API_KEY, sizeof(API_KEY), config) != NULL) {
            API_KEY[strcspn(API_KEY, "\n")] = 0;
            API_KEY[strcspn(API_KEY, "\r")] = 0;
            
            char *end = API_KEY + strlen(API_KEY) - 1;
            while(end > API_KEY && isspace((unsigned char)*end)) end--;
            *(end + 1) = '\0';
            
            fclose(config);
            printf(GREEN "    ✓ API key loaded from config file\n" RESET);
            return;
        }
        fclose(config);
    }
    
    printf(YELLOW "\n    🔑 Groq API Key Setup (100%% FREE!)\n" RESET);
    printf("    ════════════════════════════════════════════\n");
    printf("    To use Puppy Dev FREE, get an API key from:\n");
    printf("    " CYAN "https://console.groq.com/keys\n" RESET);
    printf("    " GREEN "✓ No credit card required!\n" RESET);
    printf("    " GREEN "✓ Completely free forever!\n" RESET);
    printf("\n");
    printf("    Enter your Groq API key: ");
    
    if (fgets(API_KEY, sizeof(API_KEY), stdin) == NULL) {
        printf(RED "    ✗ Failed to read API key\n" RESET);
        exit(1);
    }
    
    API_KEY[strcspn(API_KEY, "\n")] = 0;
    API_KEY[strcspn(API_KEY, "\r")] = 0;
    
    char *end = API_KEY + strlen(API_KEY) - 1;
    while(end > API_KEY && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    config = fopen("puppy_config.txt", "w");
    if (config != NULL) {
        fprintf(config, "%s", API_KEY);
        fclose(config);
        printf(GREEN "    ✓ API key saved to puppy_config.txt\n" RESET);
    }
    
    printf("\n");
}

char* find_json_string(const char *json, const char *key) {
    static char result[MAX_RESPONSE];
    memset(result, 0, sizeof(result));
    
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char *start = strstr(json, search_key);
    if (start == NULL) return NULL;
    
    start += strlen(search_key);
    while (*start == ' ' || *start == '\t') start++;
    
    if (*start != '"') return NULL;
    start++;
    
    const char *end = start;
    int escape = 0;
    
    while (*end) {
        if (escape) {
            escape = 0;
        } else if (*end == '\\') {
            escape = 1;
        } else if (*end == '"') {
            break;
        }
        end++;
    }
    
    size_t len = end - start;
    if (len >= sizeof(result)) len = sizeof(result) - 1;
    
    strncpy(result, start, len);
    result[len] = '\0';
    
    char *src = result, *dst = result;
    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'r': *dst++ = '\r'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default: *dst++ = *src; break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    return result;
}

void escape_json_string(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 2; i++) {
        if (j >= output_size - 3) break;
        
        switch (input[i]) {
            case '"':
                output[j++] = '\\';
                output[j++] = '"';
                break;
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            case '\n':
                output[j++] = '\\';
                output[j++] = 'n';
                break;
            case '\r':
                output[j++] = '\\';
                output[j++] = 'r';
                break;
            case '\t':
                output[j++] = '\\';
                output[j++] = 't';
                break;
            case '\b':
                output[j++] = '\\';
                output[j++] = 'b';
                break;
            case '\f':
                output[j++] = '\\';
                output[j++] = 'f';
                break;
            default:
                if (input[i] < 32) continue;
                output[j++] = input[i];
                break;
        }
    }
    output[j] = '\0';
}

char* call_groq_api(const char *user_message) {
    CURL *curl;
    CURLcode res;
    Response response = {NULL, 0};
    static char error_msg[512];
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (!curl) {
        strcpy(error_msg, "Error: Could not initialize CURL");
        return error_msg;
    }
    
    char *payload = malloc(MAX_INPUT * 20);
    if (!payload) {
        curl_easy_cleanup(curl);
        strcpy(error_msg, "Error: Memory allocation failed");
        return error_msg;
    }
    
    char *escaped_msg = malloc(MAX_INPUT * 2);
    if (!escaped_msg) {
        free(payload);
        curl_easy_cleanup(curl);
        strcpy(error_msg, "Error: Memory allocation failed");
        return error_msg;
    }
    
    escape_json_string(user_message, escaped_msg, MAX_INPUT * 2);
    
    // Build OpenAI-compatible payload for Groq
    sprintf(payload, "{\"model\":\"%s\",\"messages\":[", MODEL);
    
    // Add system message
    strcat(payload, "{\"role\":\"system\",\"content\":\"You are Puppy Dev, a friendly and helpful AI assistant created by John Clyde. You help with coding, answer questions, and have pleasant conversations. You're enthusiastic like a puppy but knowledgeable like a senior developer.\"},");
    
    // Add conversation history
    for (int i = 0; i < message_count; i++) {
        char *escaped_content = malloc(strlen(conversation[i].content) * 2 + 100);
        if (escaped_content) {
            escape_json_string(conversation[i].content, escaped_content, strlen(conversation[i].content) * 2 + 50);
            
            char msg_json[MAX_INPUT * 3];
            snprintf(msg_json, sizeof(msg_json), "{\"role\":\"%s\",\"content\":\"%s\"}%s",
                    conversation[i].role,
                    escaped_content,
                    (i < message_count - 1 || strlen(user_message) > 0) ? "," : "");
            strcat(payload, msg_json);
            free(escaped_content);
        }
    }
    
    // Add current message
    if (strlen(user_message) > 0) {
        char current_msg[MAX_INPUT * 3];
        snprintf(current_msg, sizeof(current_msg), "{\"role\":\"user\",\"content\":\"%s\"}", escaped_msg);
        strcat(payload, current_msg);
    }
    
    strcat(payload, "],\"temperature\":0.7,\"max_tokens\":2048}");
    
    free(escaped_msg);
    
    // Set up headers
    struct curl_slist *headers = NULL;
    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", API_KEY);
    
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    free(payload);
    
    if (res != CURLE_OK) {
        snprintf(error_msg, sizeof(error_msg), "Connection error: %s\nTry: pkg install ca-certificates", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        if (response.data) free(response.data);
        return error_msg;
    }
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    
    if (response.data == NULL) {
        strcpy(error_msg, "Error: Empty response from API");
        return error_msg;
    }
    
    // Check for errors
    if (http_code != 200) {
        char *error_message = find_json_string(response.data, "message");
        
        if (error_message) {
            snprintf(error_msg, sizeof(error_msg), "API Error (%ld): %s\nCheck your Groq API key at console.groq.com", http_code, error_message);
        } else {
            snprintf(error_msg, sizeof(error_msg), "API Error (%ld): Check your API key", http_code);
        }
        
        free(response.data);
        return error_msg;
    }
    
    // Extract content from OpenAI-style response
    char *result = find_json_string(response.data, "content");
    static char result_copy[MAX_RESPONSE];
    
    if (result) {
        strncpy(result_copy, result, sizeof(result_copy) - 1);
        result_copy[sizeof(result_copy) - 1] = '\0';
    } else {
        strcpy(result_copy, "Woof! Sorry, I got confused. Can you try asking that again?");
    }
    
    free(response.data);
    curl_global_cleanup();
    
    return result_copy;
}

void add_to_conversation(const char *role, const char *content) {
    if (message_count >= conversation_capacity) {
        conversation_capacity = (conversation_capacity == 0) ? 10 : conversation_capacity * 2;
        Message *new_conv = realloc(conversation, conversation_capacity * sizeof(Message));
        if (!new_conv) return;
        conversation = new_conv;
    }
    
    strncpy(conversation[message_count].role, role, sizeof(conversation[message_count].role) - 1);
    conversation[message_count].content = strdup(content);
    if (conversation[message_count].content) {
        message_count++;
    }
}

void print_help() {
    printf(CYAN "\n    📚 PUPPY DEV COMMANDS:\n" RESET);
    printf("    ════════════════════════════════════════════\n");
    printf("    " GREEN "/help" RESET "      - Show this help menu\n");
    printf("    " GREEN "/clear" RESET "     - Clear conversation history\n");
    printf("    " GREEN "/history" RESET "   - Show conversation history\n");
    printf("    " GREEN "/save" RESET "      - Save conversation to file\n");
    printf("    " GREEN "/test" RESET "      - Test API connection\n");
    printf("    " GREEN "/about" RESET "     - About Puppy Dev\n");
    printf("    " GREEN "/exit" RESET "      - Exit Puppy Dev\n");
    printf("\n");
}

void test_api() {
    printf(YELLOW "\n    🔍 Testing FREE API connection...\n" RESET);
    print_puppy_thinking();
    
    char *response = call_groq_api("Say 'woof' if you can hear me!");
    
    if (strstr(response, "Error") || strstr(response, "error")) {
        printf(RED "    ✗ API Test Failed:\n" RESET);
        printf("    %s\n\n", response);
        printf(YELLOW "    💡 Get your FREE Groq API key:\n" RESET);
        printf("    1. Visit: " CYAN "https://console.groq.com/keys\n" RESET);
        printf("    2. Sign up (no credit card needed!)\n");
        printf("    3. Create API key\n");
        printf("    4. Paste it when asked\n\n");
    } else {
        printf(GREEN "    ✓ FREE API connection successful!\n" RESET);
        printf("    " PINK "🐶 Response: " RESET "%s\n\n", response);
    }
}

void print_about() {
    printf(PINK "\n    ╔══════════════════════════════════════════════════╗\n");
    printf("    ║           " BOLD "ABOUT PUPPY DEV" RESET PINK "                        ║\n");
    printf("    ╚══════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    printf("    🐕 " BOLD "Name:" RESET " Puppy Dev\n");
    printf("    👨‍💻 " BOLD "Developer:" RESET " %s\n", DEVELOPER);
    printf("    📦 " BOLD "Version:" RESET " %s\n", VERSION);
    printf("    🤖 " BOLD "AI Engine:" RESET " Groq (Llama 3.3 70B)\n");
    printf("    💰 " BOLD "Cost:" RESET " " GREEN "100%% FREE FOREVER!\n" RESET);
    printf("\n");
    printf("    " CYAN "Features:\n" RESET);
    printf("    • " GREEN "✓" RESET " Completely free AI\n");
    printf("    • " GREEN "✓" RESET " No credit card required\n");
    printf("    • " GREEN "✓" RESET " Fast responses\n");
    printf("    • " GREEN "✓" RESET " Coding assistance\n");
    printf("    • " GREEN "✓" RESET " Context memory\n");
    printf("    • " GREEN "✓" RESET " Works on Termux\n");
    printf("\n");
    printf("    " YELLOW "Created with ❤️  by %s\n" RESET, DEVELOPER);
    printf("\n");
}

void print_response(const char *response) {
    printf("    " PINK "🐶 Puppy: " RESET);
    
    const char *line = response;
    int line_length = 0;
    int max_length = 60;
    
    while (*line) {
        if (*line == '\n') {
            printf("\n            ");
            line_length = 0;
            line++;
            continue;
        }
        
        if (line_length >= max_length && *line == ' ') {
            printf("\n            ");
            line_length = 0;
            line++;
            continue;
        }
        
        printf("%c", *line);
        line_length++;
        line++;
    }
    
    printf("\n\n");
}

void save_conversation() {
    char filename[100];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    sprintf(filename, "puppy_chat_%04d%02d%02d_%02d%02d%02d.txt",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
    
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        printf(RED "    ✗ Error: Could not save conversation\n" RESET);
        return;
    }
    
    fprintf(file, "PUPPY DEV CONVERSATION\n");
    fprintf(file, "Date: %s", ctime(&now));
    fprintf(file, "═══════════════════════════════════════════\n\n");
    
    for (int i = 0; i < message_count; i++) {
        if (strcmp(conversation[i].role, "user") == 0) {
            fprintf(file, "You: %s\n\n", conversation[i].content);
        } else {
            fprintf(file, "Puppy Dev: %s\n\n", conversation[i].content);
        }
    }
    
    fclose(file);
    printf(GREEN "    ✓ Conversation saved to: %s\n" RESET, filename);
    printf("\n");
}

int main() {
    char input[MAX_INPUT];
    
    print_banner();
    setup_api_key();
    
    printf(GREEN "    ✓ Puppy Dev is ready!\n" RESET);
    printf("    " BOLD "💚 100%% FREE • No credit card needed!\n" RESET);
    printf("    " DIM "Type '/help' for commands or '/test' to test API\n" RESET);
    printf("\n");
    
    while (1) {
        printf("    " BLUE "You: " RESET);
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) continue;
        
        if (strcmp(input, "/exit") == 0 || strcmp(input, "/quit") == 0) {
            printf("\n    " PINK "🐶 Woof! Thanks for chatting! See you later! 🐾\n" RESET "\n");
            break;
        }
        else if (strcmp(input, "/help") == 0) {
            print_help();
            continue;
        }
        else if (strcmp(input, "/test") == 0) {
            test_api();
            continue;
        }
        else if (strcmp(input, "/about") == 0) {
            print_about();
            continue;
        }
        else if (strcmp(input, "/clear") == 0) {
            for (int i = 0; i < message_count; i++) {
                free(conversation[i].content);
            }
            message_count = 0;
            printf(GREEN "    ✓ Conversation cleared!\n" RESET "\n");
            continue;
        }
        else if (strcmp(input, "/save") == 0) {
            save_conversation();
            continue;
        }
        
        print_puppy_thinking();
        add_to_conversation("user", input);
        
        char *response = call_groq_api(input);
        add_to_conversation("assistant", response);
        print_response(response);
    }
    // Cleanup
    for (int i = 0; i < message_count; i++) {
        free(conversation[i].content);
    }
    free(conversation);
    return 0;
}
