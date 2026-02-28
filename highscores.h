#ifndef HIGHSCORES_H
#define HIGHSCORES_H

#define MAX_SCORES 10

typedef struct {
    char name[32];
    int score;
    int level;
} HighScore;

int load_scores(HighScore scores[]);
void save_scores(HighScore scores[], int count);
void add_score(HighScore scores[], int *count,
               const char *name, int score, int level);
void show_scores(const HighScore scores[], int count);
int highscore_rank(HighScore scores[], int count, int score);

#endif
