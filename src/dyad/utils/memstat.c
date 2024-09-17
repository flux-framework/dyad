#if defined(DYAD_HAS_CONFIG)
#include <dyad/dyad_config.hpp>
#else
#error "no config"
#endif

#include <dyad/utils/memstat.h>
#include <sys/resource.h> // getrusage ()
#include <linux/limits.h> // PATH_MAX
#include <unistd.h> // sysconf(_SC_PAGESIZE)
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    unsigned long m_size, m_resident, m_share, m_text, m_lib, m_data, m_dt;
} statm_t;

// https://docs.kernel.org/filesystems/proc.html
static int get_mem_status (statm_t* ms)
{
    unsigned long dummy;
    const char* statm_path = "/proc/self/statm";

    FILE *f = fopen (statm_path, "r");

    if (!ms || !f) {
        perror (statm_path);
        return EXIT_FAILURE;
    }
    if (7 != fscanf (f, "%lu %lu %lu %lu %lu %lu %lu",
                     &(ms->m_size), &(ms->m_resident), &(ms->m_share),
                     &(ms->m_text), &(ms->m_lib), &(ms->m_data), &(ms->m_dt)))
    {
        perror (statm_path);
        return EXIT_FAILURE;
    }
    fclose (f);
    return EXIT_SUCCESS;
}

int print_mem_status (const char* note, char* buf, unsigned bufsize)
{
    statm_t ms;
    const char* nt = (!note ? "" : note);
    if (get_mem_status (&ms) != EXIT_SUCCESS) return 0;

    if (!buf) {
        printf ("%s:\tsize %lu\tresident %lu\tshare %lu\ttext %lu"
                "\tlib %lu\tdata %lu\tdt %lu (in # pages of %ld)\n",
                nt, ms.m_size, ms.m_resident, ms.m_share, ms.m_text,
                ms.m_lib, ms.m_data, ms.m_dt, sysconf (_SC_PAGESIZE));
    } else {
        return snprintf (buf, bufsize, "%s:\tsize %lu\tresident %lu\tshare %lu\t"
                         "text %lu\tlib %lu\tdata %lu\tdt %lu (in # pages of %ld)\n",
                         nt, ms.m_size, ms.m_resident, ms.m_share, ms.m_text,
                         ms.m_lib, ms.m_data, ms.m_dt, sysconf (_SC_PAGESIZE));
    }
    return 0;
}

// https://man7.org/linux/man-pages/man2/getrusage.2.html
int print_rusage (const char* note, char* buf, unsigned bufsize)
{
    struct rusage r_usage;
    getrusage (RUSAGE_SELF,&r_usage);

    if (!buf) {
        printf ("%s:\n"
                "Memory usage: %ld kilobytes\n"
                "page reclaims: %ld\n"
                "page faults: %ld\n"
                "block input ops: %ld\n"
                "block input ops: %ld\n"
                "voluntary context switches: %ld\n"
                "involuntary context switches: %ld\n",
                (!note ? "" : note),
                r_usage.ru_maxrss,
                r_usage.ru_minflt,
                r_usage.ru_majflt,
                r_usage.ru_inblock,
                r_usage.ru_oublock,
                r_usage.ru_nvcsw,
                r_usage.ru_nivcsw);
    } else {
        int n = 0;
        n = snprintf (buf, bufsize, "%s:\n"
                      "Memory usage: %ld kilobytes\n"
                      "page reclaims: %ld\n"
                      "page faults: %ld\n"
                      "block input ops: %ld\n"
                      "block input ops: %ld\n"
                      "voluntary context switches: %ld\n"
                      "involuntary context switches: %ld\n",
                      (!note ? "" : note),
                      r_usage.ru_maxrss,
                      r_usage.ru_minflt,
                      r_usage.ru_majflt,
                      r_usage.ru_inblock,
                      r_usage.ru_oublock,
                      r_usage.ru_nvcsw,
                      r_usage.ru_nivcsw);
        return n;
    }
    return 0;
}

int print_rusage_in_a_line (const char* note, char* buf, unsigned bufsize)
{
    struct rusage r_usage;
    getrusage (RUSAGE_SELF, &r_usage);

    if (!buf) {
        printf ("%s\tmemory usage: %ld %ld %ld %ld %ld %ld %ld\n",
                (!note ? "" : note),
                r_usage.ru_maxrss,
                r_usage.ru_minflt,
                r_usage.ru_majflt,
                r_usage.ru_inblock,
                r_usage.ru_oublock,
                r_usage.ru_nvcsw,
                r_usage.ru_nivcsw);
    } else {
        int n = 0;
        n = snprintf (buf, bufsize, "%s\tmemory usage: %ld %ld %ld %ld %ld %ld %ld\n",
                      (!note ? "" : note),
                      r_usage.ru_maxrss,
                      r_usage.ru_minflt,
                      r_usage.ru_majflt,
                      r_usage.ru_inblock,
                      r_usage.ru_oublock,
                      r_usage.ru_nvcsw,
                      r_usage.ru_nivcsw);
        return n;
    }
    return 0;
}
