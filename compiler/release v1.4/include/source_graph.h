#ifndef NEVO_SOURCE_GRAPH_H
#define NEVO_SOURCE_GRAPH_H

typedef struct
{
    char **paths;
    int count;
} NevoSourceList;

int nevo_collect_sources(int root_count, char **root_paths,
                         NevoSourceList *sources);
void nevo_free_source_list(NevoSourceList *sources);

#endif
