#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "highscores.h"

#define SCORE_FILE ".robots_scores"
#define MAX_PATH 512

static void get_score_path(char *buf, size_t size) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, size, "%s/%s", home, SCORE_FILE);
}

int load_scores(HighScore scores[]) {
    char path[MAX_PATH];
    get_score_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int count = 0;
    while (count < MAX_SCORES && fscanf(f, "%31s %d %d", scores[count].name, &scores[count].score, &scores[count].level) == 3) {
        count++;
    }
    fclose(f);
    return count;
}

void save_scores(HighScore scores[], int count) {
    char path[MAX_PATH];
    get_score_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d %d\n", scores[i].name, scores[i].score, scores[i].level);
    }
    fclose(f);
}

void add_score(HighScore scores[], int *count, const char *name, int score, int level) {
    if (*count < MAX_SCORES) {
        strcpy(scores[*count].name, name);
        scores[*count].score = score;
        scores[*count].level = level;
        (*count)++;
    } else if (score > scores[*count - 1].score) {
        strcpy(scores[*count - 1].name, name);
        scores[*count - 1].score = score;
        scores[*count - 1].level = level;
    }
    for (int i = 0; i < *count - 1; i++) {
        for (int j = i + 1; j < *count; j++) {
            if (scores[j].score > scores[i].score) {
                HighScore tmp = scores[i];
                scores[i] = scores[j];
                scores[j] = tmp;
            }
        }
    }
}

void show_scores(const HighScore scores[], int count) {
    printf("\n=== HIGH SCORES ===\n");
    for (int i = 0; i < count; i++) {
        printf("%2d. %-10s %6d (level %d)\n", i + 1, scores[i].name, scores[i].score, scores[i].level);
    }
    printf("===================\n\n");
}

int highscore_rank(HighScore scores[], int count, int score) {
    int i = 0;
    while (i < count && scores[i].score > score) {
        i++;
    }
    return ++i;
}