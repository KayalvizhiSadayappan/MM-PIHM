#include "pihm.h"

void ReadGeol(const char *fn, geoltbl_struct *geoltbl)
{
    FILE           *fp;
    int             i;
    char            cmdstr[MAXSTRING];
    int             match;
    int             index;
    int             lno = 0;

    fp = pihm_fopen(fn, "r");
    pihm_printf(VL_VERBOSE, " Reading %s\n", fn);

    /* Start reading soil file */
    NextLine(fp, cmdstr, &lno);
    ReadKeyword(cmdstr, "NUMGEOL", 'i', fn, lno, &geoltbl->number);

    geoltbl->ksatv  = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->ksath  = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->smcmax = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->smcmin = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->alpha  = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->beta   = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->areafh = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->areafv = (double *)malloc(geoltbl->number * sizeof (double));
    geoltbl->dmac   = (double *)malloc(geoltbl->number * sizeof (double));

    /* Skip header line */
    NextLine (fp, cmdstr, &lno);

    for (i = 0; i < geoltbl->number; i++)
    {
        NextLine(fp, cmdstr, &lno);
        match = sscanf(cmdstr, "%d %lf %lf %lf %lf %lf %lf %lf %lf %lf",
            &index, &geoltbl->ksatv[i], &geoltbl->ksath[i],
            &geoltbl->smcmax[i], &geoltbl->smcmin[i],
            &geoltbl->alpha[i], &geoltbl->beta[i],
            &geoltbl->areafh[i], &geoltbl->areafv[i],
            &geoltbl->dmac[i]);

        if (match != 10 || i != index - 1)
        {
            pihm_printf(VL_ERROR,
                "Error reading properties of the %dth geology type.\n", i + 1);
            pihm_printf(VL_ERROR, "Error in %s near Line %d.\n", fn, lno);
            pihm_exit(EXIT_FAILURE);
        }
    }

    NextLine(fp, cmdstr, &lno);
    ReadKeyword(cmdstr, "KMACV_RO", 'd', fn, lno, &geoltbl->kmacv_ro);

    NextLine(fp, cmdstr, &lno);
    ReadKeyword(cmdstr, "KMACH_RO", 'd', fn, lno, &geoltbl->kmach_ro);

    fclose (fp);
}
