#include "pihm.h"

void ReadBedrock(const char fn[], meshtbl_struct *meshtbl,
    atttbl_struct *atttbl, ctrl_struct *ctrl)
{
    FILE           *fp;
    int             i;
    char            cmdstr[MAXSTRING];
    int             match;
    int             index;
    int             lno = 0;

    fp = pihm_fopen(fn, "r");
    pihm_printf(VL_VERBOSE, " Reading %s\n", fn);

    /* Start reading bedrock file */
    /* Read fbr boundary conditions */
    atttbl->fbr_bc = (int **)malloc(nelem * sizeof(int *));
    for (i = 0; i < nelem; i++)
    {
        atttbl->fbr_bc[i] = (int *)malloc(NUM_EDGE * sizeof(int));
    }

    /* Skip header line */
    NextLine(fp, cmdstr, &lno);
    for (i = 0; i < nelem; i++)
    {
        NextLine(fp, cmdstr, &lno);
        match = sscanf(cmdstr, "%d %d %d %d", &index,
            &atttbl->fbr_bc[i][0], &atttbl->fbr_bc[i][1],
            &atttbl->fbr_bc[i][2]);
        if (match != 4 || i != index - 1)
        {
            pihm_printf(VL_ERROR,
                "Error reading boundary condition type for fractured bedrock"
                "layer of the %dth element.\n", i + 1);
            pihm_printf(VL_ERROR, "Error in %s near Line %d.\n", fn, lno);
            pihm_exit(EXIT_FAILURE);
        }
    }

    /* Read bedrock elevations */
    meshtbl->zbed = (double *)malloc(meshtbl->numnode * sizeof (double));

    /* Skip header line */
    NextLine(fp, cmdstr, &lno);
    for (i = 0; i < meshtbl->numnode; i++)
    {
        NextLine(fp, cmdstr, &lno);
        match = sscanf(cmdstr, "%d %lf", &index, &meshtbl->zbed[i]);
        if (match != 2 || i != index - 1)
        {
            pihm_printf(VL_ERROR,
                "Error reading bedrock description of the %dth node.\n", i + 1);
            pihm_printf(VL_ERROR, "Error in %s near Line %d.\n",
                fn, lno);
            pihm_exit(EXIT_FAILURE);
        }
    }

    NextLine(fp, cmdstr, &lno);
    ctrl->prtvrbl[FBRUNSAT_CTRL] = ReadPrintCtrl(cmdstr, "DEEPUNSAT", fn, lno);

    NextLine(fp, cmdstr, &lno);
    ctrl->prtvrbl[FBRGW_CTRL] = ReadPrintCtrl(cmdstr, "DEEPGW", fn, lno);

    NextLine(fp, cmdstr, &lno);
    ctrl->prtvrbl[FBRINFIL_CTRL] = ReadPrintCtrl(cmdstr, "DEEPINFIL", fn, lno);

    NextLine (fp, cmdstr, &lno);
    ctrl->prtvrbl[FBRRECHG_CTRL] = ReadPrintCtrl(cmdstr, "DEEPRECHG", fn, lno);

    NextLine (fp, cmdstr, &lno);
    ctrl->prtvrbl[FBRFLOW_CTRL] = ReadPrintCtrl(cmdstr, "DEEPFLOW", fn, lno);

    fclose (fp);
}
