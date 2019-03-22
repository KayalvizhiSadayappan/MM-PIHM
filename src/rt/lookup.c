#include "pihm.h"

#define LINE_WIDTH 512
#define WORDS_LINE 40
#define WORD_WIDTH 80

void Lookup(FILE *database, const calib_struct *cal, chemtbl_struct chemtbl[],
    kintbl_struct kintbl[], rttbl_struct *rttbl)
{
    double          tmpval[WORDS_LINE];
    /* Kinetic reactions is currently only applicable to minerals */
    int             i, j, k, l, keq_position = 0, total_temp_points;
    int             mn, in;     // 08.19 Wei
    char            line[LINE_WIDTH];
    char          **tmpstr = (char **)malloc(WORDS_LINE * sizeof(char *));

    for (i = 0; i < WORDS_LINE; i++)
    {
        tmpstr[i] = (char *)malloc(WORD_WIDTH * sizeof(char));
        memset(tmpstr[i], 0, WORD_WIDTH);
    }

    /*
     * Find temperature point in databse
     */
    rewind(database);
    fgets(line, LINE_WIDTH, database);
    while (keymatch(line, "'temperature points'", tmpval, tmpstr) != 1)
    {
        fgets(line, LINE_WIDTH, database);
    }
    total_temp_points = tmpval[0];
    for (i = 0; i < tmpval[0]; i++)
    {
        if (tmpval[i + 1] == rttbl->Temperature)
        {
            PIHMprintf(VL_VERBOSE,
                "\n Temperature point %6.4f C found in database!\n\n",
                tmpval[i + 1]);
            keq_position = i + 1;
        }
    }

    /*
     * Read Debye Huckel parameters from database
     */
    while (keymatch(line, "'Debye-Huckel adh'", tmpval, tmpstr) != 1)
    {
        fgets(line, LINE_WIDTH, database);
    }
    rttbl->adh = tmpval[keq_position - 1];
    while (keymatch(line, "'Debye-Huckel bdh'", tmpval, tmpstr) != 1)
    {
        fgets(line, LINE_WIDTH, database);
    }
    rttbl->bdh = tmpval[keq_position - 1];
    while (keymatch(line, "'Debye-Huckel bdt'", tmpval, tmpstr) != 1)
    {
        fgets(line, LINE_WIDTH, database);
    }
    rttbl->bdt = tmpval[keq_position - 1];

    PIHMprintf(VL_VERBOSE,
        " Debye-Huckel Parameters set to A=%6.4f; B=%6.4f; b=%6.4f\n\n",
        rttbl->adh, rttbl->bdh, rttbl->bdt);


    for (i = 0; i < MAXSPS; i++)
    {
        for (j = 0; j < MAXSPS; j++)
        {
            rttbl->Dependency[i][j] = 0.0;      /* NumSsc x NumSdc */
            rttbl->Dep_kinetic[i][j] = 0.0;     /* (NumMkr + NumAkr) x NumStc */
            rttbl->Dep_kinetic_all[i][j] = 0.0; /* NumMin x NumStc */
            rttbl->Totalconc[i][j] = 0.0;       /* NumStc x (NumStc + NumSsc) */
#if NOT_YET_IMPLEMENTED
            rttbl->Totalconck[i][j] = 0.0;      /* NumStc x (NumStc + NumSsc) */
#endif
        }
        /* Keqs of equilibrium/ kinetic and kinetic all */
        rttbl->Keq[i] = 0.0;                    /* NumSsc */
        rttbl->KeqKinect[i] = 0.0;              /* NumMkr + NumAkr */
        rttbl->KeqKinect_all[i] = 0.0;          /* NumMin */
    }

    /*
     * Update species parameters
     */
    for (i = 0; i < rttbl->NumStc + rttbl->NumSsc; i++)
    {
        if (strcmp(chemtbl[i].ChemName, "pH") == 0)
        {
            strcpy(chemtbl[i].ChemName, "H+");

        }
        wrap(chemtbl[i].ChemName);

        chemtbl[i].DiffCoe = rttbl->DiffCoe;
        chemtbl[i].DispCoe = rttbl->DispCoe;
        chemtbl[i].Charge = 0.0;
        chemtbl[i].SizeF = 1.0;
    }

    /*
     * Read parameters for primary species
     */
    rewind(database);
    fgets(line, LINE_WIDTH, database);
    while (keymatch(line, "'End of primary'", tmpval, tmpstr) != 1)
    {
        if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
        {
            for (i = 0; i < rttbl->NumStc; i++)
                if (strcmp(chemtbl[i].ChemName, tmpstr[0]) == 0)
                {
                    PIHMprintf(VL_VERBOSE,
                        " Primary species %s found in database!\n MolarMass = %6.4f\n\n",
                        chemtbl[i].ChemName, tmpval[2]);
                    chemtbl[i].MolarMass = tmpval[2];
                    chemtbl[i].Charge = tmpval[1];
                    chemtbl[i].SizeF = tmpval[0];
                }
        }
        fgets(line, LINE_WIDTH, database);
    }

    /*
     * Read parameters for secondary species
     */
    while (keymatch(line, "'End of secondary'", tmpval, tmpstr) != 1)
    {
        if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
        {
            for (i = rttbl->NumStc; i < rttbl->NumSsc + rttbl->NumStc; i++)
                if (strcmp(chemtbl[i].ChemName, tmpstr[0]) == 0)
                {
                    PIHMprintf(VL_VERBOSE,
                        " Secondary species %s found in database!\n",
                        chemtbl[i].ChemName);
                    PIHMprintf(VL_VERBOSE, " %s", line);
                    chemtbl[i].itype = AQUEOUS;
                    for (j = 0; j < WORDS_LINE; j++)
                    {
                        for (k = 0; k < rttbl->NumSdc; k++)
                            if (strcmp(chemtbl[k].ChemName,
                                    tmpstr[j]) == 0)
                                rttbl->Dependency[i - rttbl->NumStc][k] =
                                    atof(tmpstr[j - 1]);
                    }
                    rttbl->Keq[i - rttbl->NumStc] =
                        tmpval[(int)tmpval[0] + keq_position];
                    PIHMprintf(VL_VERBOSE, " Keq = %6.4f\n", rttbl->Keq[i - rttbl->NumStc]);
                    chemtbl[i].MolarMass =
                        tmpval[(int)tmpval[0] + total_temp_points + 3];
                    chemtbl[i].Charge =
                        tmpval[(int)tmpval[0] + total_temp_points + 2];
                    chemtbl[i].SizeF =
                        tmpval[(int)tmpval[0] + total_temp_points + 1];
                    PIHMprintf(VL_VERBOSE,
                        " MolarMass = %6.4f, Charge = %6.4f, SizeFactor = %6.4f\n\n",
                        chemtbl[i].MolarMass, chemtbl[i].Charge,
                        chemtbl[i].SizeF);
                }
        }
        fgets(line, LINE_WIDTH, database);
    }

    /*
     * Read parameters for minerals
     */
    while (keymatch(line, "'End of minerals'", tmpval, tmpstr) != 1)
    {
        if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
        {
            for (i = NumSpc + rttbl->NumAds + rttbl->NumCex; i < rttbl->NumStc; i++)
                if (strcmp(chemtbl[i].ChemName, tmpstr[0]) == 0)
                {
                    PIHMprintf(VL_VERBOSE, " Mineral %s found in database!\n",
                        chemtbl[i].ChemName);
                    PIHMprintf(VL_VERBOSE, " %s", line);
                    chemtbl[i].itype = MINERAL;
                    rttbl->KeqKinect_all[i - NumSpc - rttbl->NumAds -
                        rttbl->NumCex] = tmpval[(int)tmpval[1] + keq_position + 1];
                    for (j = 1; j < WORDS_LINE; j++)
                    {
                        for (k = 0; k < rttbl->NumStc + rttbl->NumSsc; k++)
                            if (strcmp(chemtbl[k].ChemName,
                                    tmpstr[j]) == 0)
                            {
                                if (k < rttbl->NumStc)
                                {
                                    rttbl->Dep_kinetic_all[i - rttbl->NumStc +
                                        rttbl->NumMin][k] = atof(tmpstr[j - 1]);
                                }
                                else
                                {
                                    for (l = 0; l < NumSpc; l++)    /* Number of primary species in the rt simulator      */
                                        rttbl->Dep_kinetic_all[i - rttbl->NumStc +
                                            rttbl->NumMin][l] +=
                                            atof(tmpstr[j -
                                                1]) * rttbl->Dependency[k -
                                            rttbl->NumStc][l];
                                    rttbl->KeqKinect_all[i - NumSpc -
                                        rttbl->NumAds - rttbl->NumCex] +=
                                        atof(tmpstr[j - 1]) * rttbl->Keq[k -
                                        rttbl->NumStc];
                                }
                            }
                    }
                    rttbl->Dep_kinetic_all[i - rttbl->NumStc + rttbl->NumMin][i] = -1.0;
                    PIHMprintf(VL_VERBOSE, " Keq = %6.4f\n",
                        rttbl->KeqKinect_all[i - NumSpc - rttbl->NumAds -
                            rttbl->NumCex]);
                    chemtbl[i].MolarMass =
                        tmpval[(int)tmpval[1] + total_temp_points + 2];
                    chemtbl[i].MolarVolume = tmpval[0];
                    chemtbl[i].Charge = 0;
                    PIHMprintf(VL_VERBOSE,
                        " MolarMass = %6.4f, MolarVolume = %6.4f\n\n",
                        chemtbl[i].MolarMass, chemtbl[i].MolarVolume);
                }
        }
        fgets(line, LINE_WIDTH, database);
    }

    /*
     * Build dependencies
     */
    for (i = 0; i < rttbl->NumMkr + rttbl->NumAkr; i++)
    {
        PIHMprintf(VL_VERBOSE,
            " Selecting the kinetic species %s from all possible species.\n\n",
            chemtbl[kintbl[i].position].ChemName);
        rttbl->KeqKinect[i] =
            rttbl->KeqKinect_all[kintbl[i].position - rttbl->NumStc + rttbl->NumMin];
        for (k = 0; k < rttbl->NumStc; k++)
        {
            rttbl->Dep_kinetic[i][k] =
                rttbl->Dep_kinetic_all[kintbl[i].position - rttbl->NumStc + rttbl->NumMin][k];
        }
    }

    while (strcmp(line, "End of surface complexation\r\n") != 1)
    {
        if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
        {
            for (i = rttbl->NumStc; i < rttbl->NumSsc + rttbl->NumStc; i++)
                if (strcmp(chemtbl[i].ChemName, tmpstr[0]) == 0)
                {
                    PIHMprintf(VL_VERBOSE,
                        " Secondary surface complexation %s found in database!\n",
                        chemtbl[i].ChemName);
                    PIHMprintf(VL_VERBOSE, " %s", line);
                    chemtbl[i].itype = ADSORPTION;
                    for (j = 0; j < WORDS_LINE; j++)
                    {
                        for (k = 0; k < rttbl->NumSdc; k++)
                            if (strcmp(chemtbl[k].ChemName,
                                    tmpstr[j]) == 0)
                                rttbl->Dependency[i - rttbl->NumStc][k] =
                                    atof(tmpstr[j - 1]);
                    }
                    rttbl->Keq[i - rttbl->NumStc] =
                        tmpval[(int)tmpval[0] + keq_position];
                    PIHMprintf(VL_VERBOSE, " Keq = %6.4f\n", rttbl->Keq[i - rttbl->NumStc]);
                }
        }
        fgets(line, LINE_WIDTH, database);
    }
    while (!feof(database))
    {
        if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
        {
            for (i = rttbl->NumStc; i < rttbl->NumSsc + rttbl->NumStc; i++)
                if (strcmp(chemtbl[i].ChemName, tmpstr[0]) == 0)
                {
                    PIHMprintf(VL_VERBOSE,
                        " Secondary ion exchange %s found in database!\n",
                        chemtbl[i].ChemName);
                    PIHMprintf(VL_VERBOSE, " %s", line);
                    chemtbl[i].itype = CATION_ECHG;
                    for (j = 0; j < WORDS_LINE; j++)
                    {
                        for (k = 0; k < rttbl->NumSdc; k++)
                            if (strcmp(chemtbl[k].ChemName,
                                    tmpstr[j]) == 0)
                                rttbl->Dependency[i - rttbl->NumStc][k] =
                                    atof(tmpstr[j - 1]);
                    }
                    rttbl->Keq[i - rttbl->NumStc] = tmpval[(int)tmpval[0] + 1];
                    PIHMprintf(VL_VERBOSE, " Keq = %6.4f \n", rttbl->Keq[i - rttbl->NumStc]);

                    rttbl->Keq[i - rttbl->NumStc] =
                        tmpval[(int)tmpval[0] + 1] + cal->Xsorption;
                    PIHMprintf(VL_VERBOSE, " After calibration: Keq = %6.4f \n",
                        rttbl->Keq[i - rttbl->NumStc]);
                }
        }
        fgets(line, LINE_WIDTH, database);
    }

    for (i = 0; i < MAXSPS; i++)
    {
        for (j = 0; j < MAXDEP; j++)
        {
            kintbl[i].dep_position[j] = 0;
            kintbl[i].monod_position[j] = 0;
            kintbl[i].inhib_position[j] = 0;
        }
    }

    for (i = 0; i < rttbl->NumMkr; i++)
    {
        rewind(database);
        fgets(line, LINE_WIDTH, database);
        while (strcmp(line, "Begin mineral kinetics\r\n") != 0)
        {
            fgets(line, LINE_WIDTH, database);
        }
        fgets(line, LINE_WIDTH, database);
        while (strcmp(line, "End of mineral kinetics\r\n") != 0)
        {
            if (keymatch(line, "NULL", tmpval, tmpstr) != 2)
            {
                wrap(tmpstr[0]);
                if (strcmp(chemtbl[kintbl[i].position].ChemName, tmpstr[0]) == 0)
                {
                    fgets(line, LINE_WIDTH, database);
                    keymatch(line, "NULL", tmpval, tmpstr);
                    if (strcmp(kintbl[i].Label, tmpstr[2]) == 0)
                    {
                        PIHMprintf(VL_VERBOSE,
                            " \n Mineral kinetics %s %s found in database!\n",
                            chemtbl[kintbl[i].position].ChemName, kintbl[i].Label);
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[2], "tst") == 0)
                            kintbl[i].type = 1;
                        if (strcmp(tmpstr[2], "PrecipitationOnly") == 0)
                            kintbl[i].type = 2;
                        if (strcmp(tmpstr[2], "DissolutionOnly") == 0)
                            kintbl[i].type = 3;
                        if (strcmp(tmpstr[2], "monod") == 0)
                            kintbl[i].type = 4;
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "rate(25C)") == 0)
                        {
                            kintbl[i].rate = tmpval[0];
                            PIHMprintf(VL_VERBOSE, " Rate is %f\n",
                                kintbl[i].rate);

                            kintbl[i].rate = tmpval[0] + cal->rate;
                            PIHMprintf(VL_VERBOSE,
                                " After calibration: Rate is %f, cal->Rate = %f \n",
                                kintbl[i].rate, cal->rate);
                        }
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "activation") == 0)
                        {
                            kintbl[i].actv = tmpval[0];
                            PIHMprintf(VL_VERBOSE, " Activation is %f\n",
                                kintbl[i].actv);
                        }
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "dependence") == 0)
                        {
                            wrap(tmpstr[2]);
                            /* Assume that all mineral kinetics only depend on
                             * one species !! */
                            kintbl[i].num_dep = 1;
                            kintbl[i].dep_position[0] = FindChem(tmpstr[2],
                                chemtbl, rttbl->NumStc);
                            kintbl[i].num_dep =
                                (kintbl[i].dep_position[0] < 0) ? 0 : 1;
                            kintbl[i].dep_power[0] = tmpval[0];
                            PIHMprintf(VL_VERBOSE, " Dependency: %s %f\n",
                                tmpstr[2], kintbl[i].dep_power[0]);
                        }

                        /* Biomass term */
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "biomass") == 0)
                        {
                            wrap(tmpstr[2]);
                            PIHMprintf(VL_VERBOSE, " Biomass species: %s \n",
                                tmpstr[2]);
                            kintbl[i].biomass_position = FindChem(tmpstr[2],
                                chemtbl, rttbl->NumStc);
                            PIHMprintf(VL_VERBOSE,
                                " Biomass species position: %d \n",
                                kintbl[i].biomass_position);
                        }

                        /* Monod term */
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "num_monod") == 0)
                        {
                            kintbl[i].num_monod = tmpval[0];
                            PIHMprintf(VL_VERBOSE, " Number of monod term: %d\n",
                                kintbl[i].num_monod);
                        }
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "monod_terms") == 0)
                        {
                            for (mn = 0; mn < kintbl[i].num_monod; mn++)
                            {
                                /* Note tmpstr indexing not same with tmpval
                                 * indexing */
                                wrap(tmpstr[mn * 2 + 2]);
                                kintbl[i].monod_position[mn] =
                                    FindChem(tmpstr[mn * 2 + 2], chemtbl,
                                        rttbl->NumStc);
                                kintbl[i].monod_para[mn] = tmpval[mn + 0];
                                if (kintbl[i].monod_position[mn] < 0)
                                {
                                    PIHMprintf(VL_ERROR,
                                        "Error finding monod_terms in species "
                                        "table.\n");
                                    PIHMexit(EXIT_FAILURE);
                                }
                                else
                                {
                                    PIHMprintf(VL_VERBOSE, " Monod term: %s %f\n",
                                    chemtbl[kintbl[i].monod_position[mn]].ChemName,
                                    kintbl[i].monod_para[mn]);
                                }
                            }
                        }

                        /* Inhibition term */
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "num_inhibition") == 0)
                        {
                            kintbl[i].num_inhib = tmpval[0];
                            PIHMprintf(VL_VERBOSE, " Number of inhibition term: %d\n",
                                kintbl[i].num_inhib);
                        }
                        fgets(line, LINE_WIDTH, database);
                        keymatch(line, "NULL", tmpval, tmpstr);
                        if (strcmp(tmpstr[0], "inhibition") == 0)
                        {
                            for (in = 0; in < kintbl[i].num_inhib; in++)
                            {
                                /* Note tmpstr indexing not same with tmpval */
                                wrap(tmpstr[in * 2 + 2]);
                                kintbl[i].inhib_position[in] =
                                    FindChem(tmpstr[in * 2 + 2], chemtbl,
                                    rttbl->NumStc);
                                kintbl[i].inhib_para[in] = tmpval[in + 0];
                                if (kintbl[i].inhib_position[in] < 0)
                                {
                                    PIHMprintf(VL_ERROR,
                                        "Error finding inhibition term in "
                                        "species table.\n");
                                    PIHMexit(EXIT_FAILURE);
                                }
                                else
                                {
                                    PIHMprintf(VL_VERBOSE, " Inhibition term: %s %f\n",
                                        chemtbl[kintbl[i].inhib_position[in]].ChemName,
                                        kintbl[i].inhib_para[in]);
                                }
                            }
                        }
                    }
                }
            }
            fgets(line, LINE_WIDTH, database);
        }
    }

    for (i = 0; i < rttbl->NumStc; i++)
        rttbl->Totalconc[i][i] = 1.0;

    for (i = rttbl->NumStc; i < rttbl->NumStc + rttbl->NumSsc; i++)
        for (j = 0; j < rttbl->NumSdc; j++)
            rttbl->Totalconc[j][i] += rttbl->Dependency[i - rttbl->NumStc][j];
    if (rttbl->NumSsc > 0)
    {
        PIHMprintf(VL_VERBOSE, " \n Dependency Matrix!\n");

        PIHMprintf(VL_VERBOSE, "%-15s", " ");
        for (i = 0; i < rttbl->NumSdc; i++)
            PIHMprintf(VL_VERBOSE, "%-14s", chemtbl[i].ChemName);
        PIHMprintf(VL_VERBOSE, "\n");

        for (i = 0; i < rttbl->NumSsc; i++)    /* Number of secondary speices in the simulator */
        {
            PIHMprintf(VL_VERBOSE, " %-14s", chemtbl[i + rttbl->NumStc].ChemName);
            for (j = 0; j < rttbl->NumSdc; j++)    /* Number of independent species (others depending on these species) */
                PIHMprintf(VL_VERBOSE, "%-14.2f", rttbl->Dependency[i][j]);
            PIHMprintf(VL_VERBOSE, " %6.2f\n", rttbl->Keq[i]);
        }
    }

    PIHMprintf(VL_VERBOSE, " \n Total Concentration Matrix!\n");
    PIHMprintf(VL_VERBOSE, "%-18s", " ");
    for (i = 0; i < rttbl->NumStc + rttbl->NumSsc; i++)
        PIHMprintf(VL_VERBOSE, "%-14s", chemtbl[i].ChemName);
    PIHMprintf(VL_VERBOSE, "\n");
    for (i = 0; i < rttbl->NumStc; i++)
    {
        PIHMprintf(VL_VERBOSE, " Sum%-14s", chemtbl[i].ChemName);
        for (j = 0; j < rttbl->NumStc + rttbl->NumSsc; j++)
            PIHMprintf(VL_VERBOSE, "%-14.2f", rttbl->Totalconc[i][j]);
        PIHMprintf(VL_VERBOSE, "\n");
    }

    PIHMprintf(VL_VERBOSE, " \n Kinetic Mass Matrx!\n");
    PIHMprintf(VL_VERBOSE, "%-15s", " ");
    for (i = 0; i < rttbl->NumStc; i++)
        PIHMprintf(VL_VERBOSE, "%-14s", chemtbl[i].ChemName);
    PIHMprintf(VL_VERBOSE, "\n");
    for (j = 0; j < rttbl->NumMkr + rttbl->NumAkr; j++)
    {
        PIHMprintf(VL_VERBOSE, " %-14s",
            chemtbl[j + NumSpc + rttbl->NumAds + rttbl->NumCex].ChemName);
        for (i = 0; i < rttbl->NumStc; i++)
        {
            PIHMprintf(VL_VERBOSE, "%-14f", rttbl->Dep_kinetic[j][i]);
        }
        PIHMprintf(VL_VERBOSE, " Keq = %-6.2f\n", rttbl->KeqKinect[j]);
    }

#if NOT_YET_IMPLEMENTED
    /* Use calibration coefficient to produce new Keq values for
     * 1) CO2, 2) other kinetic reaction */
    double          Cal_PCO2 = 1.0;
    double          Cal_Keq = 1.0;
    for (i = 0; i < rttbl->NumAkr + rttbl->NumMkr; i++)
    {
        rttbl->KeqKinect[i] +=
            (!strcmp( chemtbl[i + NumSpc + rttbl->NumAds + rttbl->NumCex].ChemName,
            "'CO2(*g)'")) ? log10(Cal_PCO2) : log10(Cal_Keq);
    }

    PIHMprintf(VL_VERBOSE, "\n Kinetic Mass Matrx (calibrated Keq)! \n");
    PIHMprintf(VL_VERBOSE, "%-15s", " ");
    for (i = 0; i < rttbl->NumStc; i++)
        PIHMprintf(VL_VERBOSE, "%-14s", chemtbl[i].ChemName);
    PIHMprintf(VL_VERBOSE, "\n");
    for (j = 0; j < rttbl->NumMkr + rttbl->NumAkr; j++)
    {
        PIHMprintf(VL_VERBOSE, " %-14s",
            chemtbl[j + NumSpc + rttbl->NumAds + rttbl->NumCex].ChemName);
        for (i = 0; i < rttbl->NumStc; i++)
        {
            PIHMprintf(VL_VERBOSE, "%-14.2f", rttbl->Dep_kinetic[j][i]);
        }
        PIHMprintf(VL_VERBOSE, " Keq = %-6.2f\n", rttbl->KeqKinect[j]);
    }
    PIHMprintf(VL_VERBOSE, "\n");
#endif

    PIHMprintf(VL_VERBOSE,
        " \n Mass action species type determination (0: immobile, 1: mobile, 2: Mixed) \n");
    for (i = 0; i < NumSpc; i++)
    {
        chemtbl[i].mtype = (chemtbl[i].itype == AQUEOUS) ?
            MOBILE_MA : IMMOBILE_MA;

        for (j = 0; j < rttbl->NumStc + rttbl->NumSsc; j++)
        {
            chemtbl[i].mtype = (rttbl->Totalconc[i][j] != 0 &&
                chemtbl[j].itype != chemtbl[i].mtype) ?
                MIXED_MA : chemtbl[i].mtype;
        }
        PIHMprintf(VL_VERBOSE, " %12s\t%10d\n",
            chemtbl[i].ChemName, chemtbl[i].mtype);
    }

    PIHMprintf(VL_VERBOSE,
        " \n Individual species type determination "
        "(1: aqueous, 2: adsorption, 3: ion exchange, 4: solid)\n");
    for (i = 0; i < rttbl->NumStc + rttbl->NumSsc; i++)
    {
        PIHMprintf(VL_VERBOSE, " %12s\t%10d\n",
            chemtbl[i].ChemName, chemtbl[i].itype);
    }

    for (i = 0; i < WORDS_LINE; i++)
    {
        free(tmpstr[i]);
    }
    free(tmpstr);
}

void wrap(char *str)
{

    char            word[WORD_WIDTH];
    sprintf(word, "'%s'", str);
    strcpy(str, word);
}

