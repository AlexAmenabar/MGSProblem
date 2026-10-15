import sys

import numpy as np
import math
from ortools.sat.python import cp_model
import matplotlib.pyplot as plt
import csv
import json

'''
Problem description
    - Homogeneous cluster: all nodes are equals and all GPUs have the same computing capacity
    - Fixed jobs: the number of resources a job can use is fixed
    - The execution time of a job is just: C x (1 - S) / rqR + C x S
'''


# Functions to load JSONs

# load workload from file to JSON 
def load_workload_json(filename):
    """
    Load a workflow JSON file.

    Returns
    -------
    workflows : list
        Original workflow-level information.
    jobs : list
        Flattened list containing all jobs from all workflows.
    """

    with open(filename, "r") as f:
        data = json.load(f)

    workflows = data["workflows"]
    jobs = []

    job_id = 0

    for workflow_id, workflow in enumerate(workflows):

        for local_job_id, job in enumerate(workflow["jobs"]):

            jobs.append({
                "id": job_id,
                "workflow": workflow_id,
                "local_id": local_job_id,

                "O": job["O"],
                "IT": job["IT"],

                "C": job["C"],
                "M": job["M"],
                "minR": job["minR"],
                "maxR": job["maxR"],

                "CPT": job["CPT"],
                "meanR": job["meanR"],

                "totalC": job["totalC"],
                "totalM": job["totalM"],

                "S": job["S"]
            })

            job_id += 1

    return {
        "nWorkflows": data["nWorkflows"],
        "pWorkflows": data["pWorkflows"],
        "deps": data["deps"],
        "workflows": workflows,
        "jobs": jobs
    }


# load workload from file to json
def load_cluster_json(filename):
    """
    Load a cluster topology JSON file.

    Returns
    -------
    cluster : dict
        Cluster and GPU topology information.
    """

    with open(filename, "r") as f:
        data = json.load(f)

    return data

def scheduling_RIGID_problem(workloadJSON, clusterJSON, resultsFile, solutionInfoFile, resultsFigure, timeout):

    rng = np.random.default_rng(42)


    # load problem data from json structures
    J = workloadJSON["nWorkflows"]
    T = 0
    
    # initialize arrays
    C = [0 for j in range(J)]
    M = [0 for j in range(J)]
    IT = [0 for j in range(J)]
    S = [0.0 for j in range(J)]
    l = [0 for j in range(J)]
    
    minR = [0 for j in range(J)]
    maxR = [0 for j in range(J)]
    rqR = [0 for j in range(J)]

    Tcomp = [0 for j in range(J)]
    TcommMperGPU = [0 for j in range(J)]

    workflows = workloadJSON["workflows"]
    for j in range(J):

        # we know we are working with workflows of individual jobs
        job = workflows[j]["jobs"][0]

        
        l[j] = workflows[j]["l"]
        IT[j] = job["IT"]

        C[j] = [0 for i in range(IT[j])]
        minR[j] = [0 for i in range(IT[j])]
        maxR[j] = [0 for i in range(IT[j])]
        S[j] = [0 for i in range(IT[j])]
        rqR[j] = [0 for i in range(IT[j])]

        for i in range(IT[j]):

            C[j][i] = job["C"][i]
            minR[j][i] = job["minR"][i] # do not consider iterations by now
            maxR[j][i] = job["maxR"][i] # do not consider iterations by now
            rqR[j][i] = maxR[j][i]
            S[j][i] = job["S"][i]

            Cp = math.ceil(C[j][i] * (S[j][i]))
            Cs = math.ceil(C[j][i] * (1 - S[j][i]))

            # in this case we can directly compute the computation time of the job
            Tcomp[j] = Tcomp[j] + math.ceil((Cp + rqR[j][i] - 1) / rqR[j][i] + Cs)
            #Tcomp[j] += math.ceil(C[j][i] * (S[j][i]) / rqR[j][i] + C[j][i] * (1 - S[j][i]))
            M[j] = M[j] + job["M"][i]


            T = T + Tcomp[j] + job["M"][i]


        print(Tcomp[j])
        print(M[j])

    # cluster (number of GPUs)
    N = clusterJSON["N"]
    R = clusterJSON["nGPUs"]

    # load latencies and bandwidths
    L = [[0 for _ in range(R)] for _ in range(R)]
    BW = [[0 for _ in range(R)] for _ in range(R)]

    # intra node latencies and BW
    nodes = clusterJSON["nodes"]
    off = 0
    maxLatencies = [0 for _ in range(N)]
    for n in range(N):

        # get node information
        node = nodes[n]

        # loop over the GPUs in the node
        for gpu1 in range(node["nGPUs"]):
            for gpu2 in range(node["nGPUs"]):
                
                if gpu1 == gpu2:
                    continue
                    
                L[off + gpu1][off + gpu2] = node["gpusLatencies"][gpu1][gpu2]
                BW[off + gpu1][off + gpu2] = node["gpusBandwidths"][gpu1][gpu2]

                if L[off + gpu1][off + gpu2] > maxLatencies[n]:
                    maxLatencies[n] = L[off + gpu1][off + gpu2]

        off += node["nGPUs"]


    # loop over nodes and compute inter-node latencies and bandwidths
    off1 = 0
    off2 = 0
    for n1 in range(N):

        off2 = 0
        for n2 in range(N):

            node1 = nodes[n1]
            node2 = nodes[n2]

            if(n1 == n2):
                off2 = off2 + node1["nGPUs"]
                continue


            # compute latency from the GPUs in the node 1 to the node 2
            latency = maxLatencies[n1] + maxLatencies[n2] + clusterJSON["latencies"][n1][n2]
            bandwidth = clusterJSON["bandwidths"][n1][n2]
            
            # fill information in the matrix
            for gpu1 in range(node1["nGPUs"]):
                for gpu2 in range(node2["nGPUs"]):

                    L[off1 + gpu1][off2 + gpu2] = latency
                    BW[off1 + gpu1][off2 + gpu2] = bandwidth
            
            
            off2 = off2 + node2["nGPUs"]

        off1 = off1 + node1["nGPUs"]

    # compute T
    #T =  # TODO, this is temporal

    ## create model
    model = cp_model.CpModel()

    ## initialize decision variables
    A = {}
    cA = {}

    s = {}
    d = {}
    e = {}
    interval = {}

    # Tcomm computation step by step
    MPerGPU = {}
    PxBW = {}
    Ttransfer = {}
    TtransferLat = {}
    Tactive = {}
    Tcomm = {}

    # define variables
    for i in range(J):

        # Start/end
        s[i] = model.NewIntVar(0, T, f"s[{i}]")
        d[i] = model.NewIntVar(0, T, f"d[{i}]")
        e[i] = model.NewIntVar(0, T, f"e[{i}]")

        MPerGPU[i] = model.NewIntVar(0, T, f"MPerGPU[{i}]")
        Tcomm[i] = model.NewIntVar(0,T,f"Tcomm[{i}]")

        for r in range(R):

            A[i, r] = model.NewBoolVar(f"A[{i},{r}]")
            interval[i,r] = model.NewOptionalIntervalVar(s[i],d[i],e[i],A[i, r],f"I_{i}_{r}")

            for ss in range(R):

                if r==ss:
                    continue

                cA[i,r,ss] = model.NewIntVar(0,1,f"cA[{i},{r},{ss}]")

                Ttransfer[i,r,ss] = model.NewIntVar(0, T,f"Ttransfer[{i},{r},{ss}]")
                TtransferLat[i,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{r},{ss}]")
                Tactive[i,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{r},{ss}]")
                PxBW[i,r,ss] = model.NewIntVar(1,T,f"PxBW[{i},{r},{ss}]")

    # define the makespan
    makespan = model.NewIntVar(0, T, "makespan")
    for i in range(J):
        model.Add(makespan >= e[i])

    ## objective function: minimize the makespan
    model.Minimize(makespan)

    ## problem constraints
    for i in range(J):

        active_pairs = []

        # job cannot start before its launch time
        model.Add(s[i] >= l[i])

        # rqR = sum(A) -- assigned resources and requested must be coherent
        model.Add(rqR[i][0] == sum(A[i, r] for r in range(R)))

        # e = s + d
        model.Add(e[i] == s[i] + d[i])

        # M / rqR
        #model.AddDivisionEquality(MPerGPU[i],M[i],rqR[i][0])

        for r in range(R):
            for ss in range(R):

                if r == ss:
                    continue

                # (M / rqR) / BW
                #model.AddDivisionEquality(Ttransfer[i,r,ss],MPerGPU[i],BW[r][ss])
                
                # topology
                model.AddElement(cA[i,r,ss],[0, Ttransfer[i,r,ss]],Tactive[i,r,ss])
                active_pairs.append(Tactive[i,r,ss])

                # cA = A x A
                model.AddMultiplicationEquality(cA[i, r, ss],[A[i, r],A[i, ss]])
            
                # P x BW
                model.AddMultiplicationEquality(PxBW[i, r, ss],[rqR[i][0],BW[r][ss]])

                # M / P x BW
                model.AddDivisionEquality(Ttransfer[i,r,ss],M[i] + PxBW[i,r,ss] - 1,PxBW[i,r,ss])

        model.AddMaxEquality(Tcomm[i], active_pairs)

        # d[i] computation
        model.Add(d[i] == Tcomp[i] + Tcomm[i])

    # intervals
    for r in range(R):
        model.AddNoOverlap([interval[i, r] for i in range(J)])


    # solve problem
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = timeout  #* 60 * 24 # 1 day
    solver.parameters.num_search_workers = 16
    print(model.Validate())
    status = solver.Solve(model)

    # ============================================================
    # Results
    # ============================================================

    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)


    if status in (cp_model.OPTIMAL,cp_model.FEASIBLE):

        solution_makespan = solver.Value(makespan)
        print(f"Makespan: {solution_makespan}",flush=True)

        # save solution variables in a file
        with open(resultsFile, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow(["job","start","end","duration","resources","gpus"])

            for i in range(J):

                start = solver.Value(s[i])
                end = solver.Value(e[i])
                duration = solver.Value(d[i])

                resources = rqR[i][0]

                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, r])
                ]

                writer.writerow([
                    i,
                    start,
                    end,
                    duration,
                    resources,
                    ",".join(map(str, gpus))
                ])


        # save general solver information
        solution_info = {
            "status": solver.StatusName(status),
            "makespan": solution_makespan,
            "objective": solver.ObjectiveValue(),
            "best_bound": solver.BestObjectiveBound(),
            "jobs": J,
            "resources": R,
            "time_horizon": T
        }

        with open(solutionInfoFile, "w") as f:
            json.dump(solution_info, f, indent=4)


        # plot GPU allocation

        # add a small margin after the makespan (0.05)
        margin = max(20, int(0.05 * solution_makespan))
        plot_end = min(T, solution_makespan + margin)
        print(f"Plotting time range: 0 - {plot_end}",flush=True)


        # occupancy[r, t] = job executing on GPU r at time t
        # -1 means that the GPU is idle.
        occupancy = -np.ones((R, plot_end), dtype=int)


        # fill GPU occupancy
        for i in range(J):

            start = solver.Value(s[i])
            end = solver.Value(e[i])

            # Nothing to plot if the job starts after
            # the plotting horizon.
            if start >= plot_end:
                continue

            # Clip interval to plotting horizon.
            start_plot = max(0,start)
            end_plot = min(plot_end,end)

            for r in range(R):

                if solver.Value(A[i, r]):

                    occupancy[r,start_plot:end_plot] = i


        # plot
        fig, ax = plt.subplots(figsize=(16, 6))

        ax.imshow(
            occupancy,
            aspect="auto",
            interpolation="none",
            extent=[
                0,
                plot_end,
                R - 0.5,
                -0.5
            ]
        )

        ax.set_xlabel("Time step")
        ax.set_ylabel("GPU")

        ax.set_yticks(
            range(R)
        )

        ax.set_yticklabels(
            [f"GPU {r}" for r in range(R)]
        )

        ax.set_title(
            f"GPU Allocation (makespan = {solution_makespan})"
        )


        # vertical grid lines
        number_of_ticks = 20

        ax.set_xticks(
            np.linspace(
                0,
                plot_end,
                number_of_ticks + 1
            )
        )

        ax.grid(
            axis="x",
            linestyle="--",
            alpha=0.4
        )


        # add job labels
        for i in range(J):

            start = solver.Value(s[i])
            end = solver.Value(e[i])

            if start >= plot_end:
                continue

            end_plot = min(
                end,
                plot_end
            )

            visible_width = end_plot - start

            if visible_width < 3:
                continue

            # find GPUs assigned to this job.
            gpus = [
                r
                for r in range(R)
                if solver.Value(A[i, r])
            ]

            for r in gpus:

                ax.text(
                    (start + end_plot) / 2,
                    r,
                    f"J{i}",
                    ha="center",
                    va="center",
                    fontsize=8
                )


        # mark makespan
        ax.axvline(solution_makespan,linestyle="--",linewidth=1.5)

        ax.text(
            solution_makespan,
            R - 0.5,
            f"  makespan = {solution_makespan}",
            rotation=90,
            va="bottom",
            ha="left"
        )

        # save plot
        plt.tight_layout()
        plt.savefig(resultsFigure,dpi=300,bbox_inches="tight")
        plt.close()

    return [status, solver]


def evolving_scheduling_problem(workloadJSON, clusterJSON, resultsFile, solutionInfoFile, resultsFigure, timeout):

    rng = np.random.default_rng(42)

    # load problem data from json structures
    J = workloadJSON["nWorkflows"]
    
    # initialize arrays
    IT = [0 for j in range(J)]
    S = [0.0 for j in range(J)]
    l = [0 for j in range(J)]
    O = [0 for j in range(J)]


    C = [0 for j in range(J)]
    M = [0 for j in range(J)]
    rqR = [0 for j in range(J)]
    Tcomp = [0 for j in range(J)]

    workflows = workloadJSON["workflows"]
    for j in range(J):

        # we know we are working with workflows of individual jobs
        job = workflows[j]["jobs"][0]

        S[j] = job["S"]
        l[j] = workflows[j]["l"]
        IT[j] = job["IT"]
        O[j] = job["O"]

        C[j] = [0 for i in range(IT[j])]
        M[j] = [0 for i in range(IT[j])]
        rqR[j] = [0 for i in range(IT[j])]
        Tcomp[j] = [0 for i in range(IT[j])]

        #C[j] = job["totalC"]
        #M[j] = job["totalM"]
        #rqR[j] = job["meanR"][0]

        for i in range(IT[j]):

            C[j][i] = job["C"][i]
            M[j][i] = job["M"][i]
            rqR[j][i] = job["meanR"][i]

            # precompute
            Tcomp[j][i] = math.ceil(C[j][i] * S[j] / rqR[j][i] + (1 - S[j]) * C[j][i])

    # cluster (number of GPUs)
    N = clusterJSON["N"]
    R = clusterJSON["nGPUs"]

    # load latencies and bandwidths
    L = [[0 for _ in range(R)] for _ in range(R)]
    BW = [[0 for _ in range(R)] for _ in range(R)]

    # intra node latencies and BW
    nodes = clusterJSON["nodes"]
    off = 0
    maxLatencies = [0 for _ in range(N)]
    for n in range(N):

        # get node information
        node = nodes[n]

        # loop over the GPUs in the node
        for gpu1 in range(node["nGPUs"]):
            for gpu2 in range(node["nGPUs"]):
                
                if gpu1 == gpu2:
                    continue
                    
                L[off + gpu1][off + gpu2] = node["gpusLatencies"][gpu1][gpu2]
                BW[off + gpu1][off + gpu2] = node["gpusBandwidths"][gpu1][gpu2]

                if L[off + gpu1][off + gpu2] > maxLatencies[n]:
                    maxLatencies[n] = L[off + gpu1][off + gpu2]

        off += node["nGPUs"]


    # loop over nodes and compute inter-node latencies and bandwidths
    off1 = 0
    off2 = 0
    for n1 in range(N):

        off2 = 0
        for n2 in range(N):

            node1 = nodes[n1]
            node2 = nodes[n2]

            if(n1 == n2):
                off2 = off2 + node1["nGPUs"]
                continue


            # compute latency from the GPUs in the node 1 to the node 2
            latency = maxLatencies[n1] + maxLatencies[n2] + clusterJSON["latencies"][n1][n2]
            bandwidth = clusterJSON["bandwidths"][n1][n2]
            
            # fill information in the matrix
            for gpu1 in range(node1["nGPUs"]):
                for gpu2 in range(node2["nGPUs"]):

                    L[off1 + gpu1][off2 + gpu2] = latency
                    BW[off1 + gpu1][off2 + gpu2] = bandwidth
            
            
            off2 = off2 + node2["nGPUs"]

        off1 = off1 + node1["nGPUs"]

    # compute T
    T = 10000 # TODO, this is temporal

    ## create model
    model = cp_model.CpModel()

    ## initialize decision variables
    A = {}
    cA = {}

    s = {}
    d = {}
    e = {}
    interval = {}

    # Tcomm computation step by step
    MPerGPU = {}
    Ttransfer = {}
    TtransferLat = {}
    Tactive = {}
    Tcomm = {}

    # define variables
    for i in range(J):

        for j in range(IT[i]):
            active_pairs = []

            # Start/end
            s[i,j] = model.NewIntVar(0, T, f"s[{i},{j}]")
            d[i,j] = model.NewIntVar(0, T, f"d[{i},{j}]")
            e[i,j] = model.NewIntVar(0, T, f"e[{i},{j}]")

            MPerGPU[i,j] = model.NewIntVar(0, T, f"MPerGPU[{i},{j}]")
            Tcomm[i,j] = model.NewIntVar(0,T,f"Tcomm[{i},{j}]")

            for r in range(R):

                A[i, j, r] = model.NewBoolVar(f"A[{i},{j},{r}]")
                interval[i,j,r] = model.NewOptionalIntervalVar(s[i,j],d[i,j],e[i,j],A[i, j, r],f"I_{i}_{j}_{r}")

                for ss in range(R):

                    if r==ss:
                        continue

                    cA[i,j,r,ss] = model.NewIntVar(0,1,f"cA[{i},{j},{r},{ss}]")

                    Ttransfer[i,j,r,ss] = model.NewIntVar(0, T,f"Ttransfer[{i},{j},{r},{ss}]")
                    TtransferLat[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    Tactive[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")


    # define the makespan
    makespan = model.NewIntVar(0, T, "makespan")
    for i in range(J):
        for j in range(IT[i]):
            model.Add(makespan >= e[i,j])

    ## objective function: minimize the makespan
    model.Minimize(makespan)

    ## problem constraints
    for i in range(J):

        # Launch time
        model.Add(s[i,0] >= l[i])

        for j in range(IT[i]):

            if j > 0:
                model.Add(s[i,j] == e[i,j-1] + 1) # an iteration cannot start until the previous iteration finishes

            # rqR = sum(A)
            model.Add(rqR[i][j] == sum(A[i, j, r] for r in range(R)))

            # e = s + computation
            model.Add(e[i,j] == s[i,j] + d[i,j])

            # Tcomm computation
            model.AddDivisionEquality(MPerGPU[i,j],M[i][j],rqR[i][j])

            for r in range(R):
                for ss in range(R):

                    if r == ss:
                        continue

                    model.AddDivisionEquality(Ttransfer[i,j,r,ss],MPerGPU[i,j],BW[r][ss])
                    model.Add(TtransferLat[i,j,r,ss] == L[r][ss] + Ttransfer[i,j,r,ss])
                    #model.AddMultiplicationEquality(Tactive[i, r, ss], [cA[i, r, ss], TtransferLat[i, r, ss]])
                    model.AddElement(cA[i,j,r,ss],[0, TtransferLat[i,j,r,ss]],Tactive[i,j,r,ss])
                    active_pairs.append(Tactive[i,j,r,ss])

                    model.AddMultiplicationEquality(cA[i,j, r, ss],[A[i,j, r],A[i,j, ss]])
                    
            model.AddMaxEquality(Tcomm[i,j], active_pairs)

            # d[i] computation
            model.Add(d[i,j] == Tcomp[i][j] + Tcomm[i,j] + O[i])

    # intervals
    for r in range(R):
        model.AddNoOverlap([interval[i,j, r] for i in range(J) for j in range(IT[i])])


    # solve problem
    solver = cp_model.CpSolver()

    '''solver.parameters.max_time_in_seconds = timeout
    solver.parameters.relative_gap_limit = 0.01
    solver.parameters.num_search_workers = 16
    callback = ProgressCallback(report_interval=30.0)
    
    print(model.Validate(), flush=True)

    status = solver.SolveWithSolutionCallback(model, callback)

    #status = solver.Solve(model)'''

    # results
    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)


    if abs(solver.ObjectiveValue()) > 1e-9:
        gap = abs(solver.ObjectiveValue() - solver.BestObjectiveBound()) 
        max(1.0, abs(solver.ObjectiveValue()))
        print(f"Gap: {100.0 * gap:.4f}%")
        

    if status in (cp_model.OPTIMAL,cp_model.FEASIBLE):

        solution_makespan = solver.Value(makespan)
        print(f"Makespan: {solution_makespan}",flush=True)

        # save solution variables in a file
        with open(resultsFile, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow(["job", "iteration","start","end","duration","resources","gpus"])

            for i in range(J):

                for j in range(IT[i]):
                    start = solver.Value(s[i,j])
                    end = solver.Value(e[i,j])
                    duration = solver.Value(d[i,j])

                    resources = rqR[i][j]

                    gpus = [
                        r
                        for r in range(R)
                        if solver.Value(A[i,j, r])
                    ]

                    writer.writerow([
                        i,
                        j,
                        start,
                        end,
                        duration,
                        resources,
                        ",".join(map(str, gpus))
                    ])


        # save general solver information
        solution_info = {
            "status": solver.StatusName(status),
            "makespan": solution_makespan,
            "objective": solver.ObjectiveValue(),
            "best_bound": solver.BestObjectiveBound(),
            "jobs": J,
            "resources": R,
            "time_horizon": T
        }

        with open(solutionInfoFile, "w") as f:
            json.dump(solution_info, f, indent=4)


        # ------------------------------------------------------------
        # Plot GPU allocation
        # ------------------------------------------------------------

        # Add a small margin after the makespan.
        margin = max(20, int(0.05 * solution_makespan))
        plot_end = min(T, solution_makespan + margin)

        print(
            f"Plotting time range: 0 - {plot_end}",
            flush=True
        )

        # occupancy[r, t] = iteration executing on GPU r at time t
        #
        # -1 means that the GPU is idle.
        #
        # We use a unique ID for every (job, iteration) pair.
        iteration_id = {}
        current_id = 0

        for i in range(J):
            for j in range(IT[i]):
                iteration_id[i, j] = current_id
                current_id += 1

        occupancy = -np.ones(
            (R, plot_end),
            dtype=int
        )


        # ------------------------------------------------------------
        # Fill GPU occupancy
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                # Nothing to plot if the iteration starts after
                # the plotting horizon.
                if start >= plot_end:
                    continue

                # Clip interval to plotting horizon.
                start_plot = max(0, start)
                end_plot = min(plot_end, end)

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                # Unique ID for this iteration.
                iid = iteration_id[i, j]

                for r in gpus:
                    occupancy[
                        r,
                        start_plot:end_plot
                    ] = iid


        # ------------------------------------------------------------
        # Plot
        # ------------------------------------------------------------

        fig, ax = plt.subplots(figsize=(16, 6))

        ax.imshow(
            occupancy,
            aspect="auto",
            interpolation="none",
            extent=[
                0,
                plot_end,
                R - 0.5,
                -0.5
            ]
        )

        ax.set_xlabel("Time step")
        ax.set_ylabel("GPU")

        ax.set_yticks(
            range(R)
        )

        ax.set_yticklabels(
            [f"GPU {r}" for r in range(R)]
        )

        ax.set_title(
            f"GPU Allocation "
            f"(makespan = {solution_makespan})"
        )


        # ------------------------------------------------------------
        # Vertical grid lines
        # ------------------------------------------------------------

        number_of_ticks = 20

        ax.set_xticks(
            np.linspace(
                0,
                plot_end,
                number_of_ticks + 1
            )
        )

        ax.grid(
            axis="x",
            linestyle="--",
            alpha=0.4
        )


        # ------------------------------------------------------------
        # Add iteration labels
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                if start >= plot_end:
                    continue

                end_plot = min(
                    end,
                    plot_end
                )

                visible_width = end_plot - max(start, 0)

                # Don't put labels on extremely short intervals.
                if visible_width < 3:
                    continue

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                if not gpus:
                    continue

                # Label in the middle of the iteration.
                label_x = (
                    max(start, 0) + end_plot
                ) / 2

                for r in gpus:

                    ax.text(
                        label_x,
                        r,
                        f"J{i}-I{j}",
                        ha="center",
                        va="center",
                        fontsize=8
                    )


        # ------------------------------------------------------------
        # Mark iteration boundaries
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])

                if 0 < start < plot_end:

                    ax.axvline(
                        start,
                        linestyle=":",
                        linewidth=0.8,
                        alpha=0.5
                    )


        # ------------------------------------------------------------
        # Mark makespan
        # ------------------------------------------------------------

        ax.axvline(
            solution_makespan,
            linestyle="--",
            linewidth=1.5
        )

        ax.text(
            solution_makespan,
            R - 0.5,
            f"  makespan = {solution_makespan}",
            rotation=90,
            va="bottom",
            ha="left"
        )


        # ------------------------------------------------------------
        # Save plot
        # ------------------------------------------------------------

        plt.tight_layout()

        plt.savefig(
            resultsFigure,
            dpi=300,
            bbox_inches="tight"
        )

        plt.close()

    return [status, solver]



def scheduling_ITERATIVE_RIGID_problem(workloadJSON, clusterJSON, resultsFile, solutionInfoFile, resultsFigure, timeout, threads):


    rng = np.random.default_rng(42)

    # load problem data from json structures
    J = workloadJSON["nWorkflows"]
    
    T = 0

    # initialize arrays
    IT = [0 for j in range(J)]
    S = [0.0 for j in range(J)]
    l = [0 for j in range(J)]
    O = [0 for j in range(J)]


    C = [0 for j in range(J)]
    Cp = [0 for j in range(J)] # precompute
    Cs = [0 for j in range(J)] # precompute
    M = [0 for j in range(J)]
    minR = [0 for j in range(J)]
    maxR = [0 for j in range(J)]
    reqR = [0 for j in range(J)]

    workflows = workloadJSON["workflows"]
    for j in range(J):

        # we know we are working with workflows of individual jobs
        job = workflows[j]["jobs"][0]

        l[j] = workflows[j]["l"]
        IT[j] = job["IT"]
        O[j] = job["O"]

        C[j] = [0 for i in range(IT[j])]
        Cp[j] = [0 for i in range(IT[j])]
        Cs[j] = [0 for i in range(IT[j])]
        M[j] = [0 for i in range(IT[j])]
        minR[j] = [0 for i in range(IT[j])]
        maxR[j] = [0 for i in range(IT[j])]
        S[j] = [0.0 for i in range(IT[j])]

        for i in range(IT[j]):

            C[j][i] = job["C"][i]
            M[j][i] = job["M"][i]
            minR[j][i] = job["minR"][i] # do not consider iterations by now
            maxR[j][i] = job["maxR"][i] # do not consider iterations by now
            S[j][i] = job["S"][i]

            if i == 0:
                reqR[j] = maxR[j][i]


            Cp[j][i] = math.ceil(C[j][i] * (S[j][i]))
            Cs[j][i] = math.ceil(C[j][i] * (1 - S[j][i]))

            T = T + int(Cp[j][i] + Cs[j][i] + M[j][i] / 100)

    # cluster (number of GPUs)
    N = clusterJSON["N"]
    R = clusterJSON["nGPUs"]

    # load latencies and bandwidths
    L = [[0 for _ in range(R)] for _ in range(R)]
    BW = [[0 for _ in range(R)] for _ in range(R)]

    # intra node latencies and BW
    nodes = clusterJSON["nodes"]
    off = 0
    maxLatencies = [0 for _ in range(N)]
    for n in range(N):

        # get node information
        node = nodes[n]

        # loop over the GPUs in the node
        for gpu1 in range(node["nGPUs"]):
            for gpu2 in range(node["nGPUs"]):
                
                if gpu1 == gpu2:
                    continue
                    
                L[off + gpu1][off + gpu2] = node["gpusLatencies"][gpu1][gpu2]
                BW[off + gpu1][off + gpu2] = node["gpusBandwidths"][gpu1][gpu2]

                if L[off + gpu1][off + gpu2] > maxLatencies[n]:
                    maxLatencies[n] = L[off + gpu1][off + gpu2]

        off += node["nGPUs"]


    # loop over nodes and compute inter-node latencies and bandwidths
    off1 = 0
    off2 = 0
    for n1 in range(N):

        off2 = 0
        for n2 in range(N):

            node1 = nodes[n1]
            node2 = nodes[n2]

            if(n1 == n2):
                off2 = off2 + node1["nGPUs"]
                continue


            # compute latency from the GPUs in the node 1 to the node 2
            latency = maxLatencies[n1] + maxLatencies[n2] + clusterJSON["latencies"][n1][n2]
            bandwidth = clusterJSON["bandwidths"][n1][n2]
            
            # fill information in the matrix
            for gpu1 in range(node1["nGPUs"]):
                for gpu2 in range(node2["nGPUs"]):

                    L[off1 + gpu1][off2 + gpu2] = latency
                    BW[off1 + gpu1][off2 + gpu2] = bandwidth
            
            
            off2 = off2 + node2["nGPUs"]

        off1 = off1 + node1["nGPUs"]

    ## create model
    model = cp_model.CpModel()

    ## initialize decision variables
    A = {}
    cA = {}
    P = {}

    s = {}
    d = {}
    e = {}
    interval = {}

    # Tcomm computation step by step
    MPerGPU = {}
    PxBW = {}
    Ttransfer = {}
    TtransferLat = {}
    Tactive = {}
    Tcomm = {}

    # Tcomp computation step by step
    TcompPar = {}
    Tcomp = {}


    # define variables
    for i in range(J):

        for j in range(IT[i]):

            # Start/end
            s[i,j] = model.NewIntVar(0, T, f"s[{i},{j}]")
            d[i,j] = model.NewIntVar(1, T, f"d[{i},{j}]")
            e[i,j] = model.NewIntVar(1, T, f"e[{i},{j}]")

            P[i,j] = model.NewIntVar(minR[i][j], maxR[i][j], f"P[{i},{j}]")

            MPerGPU[i,j] = model.NewIntVar(0, T, f"MPerGPU[{i},{j}]")
            Tcomm[i,j] = model.NewIntVar(0,T,f"Tcomm[{i},{j}]")
            Tcomp[i,j] = model.NewIntVar(0,T,f"Tcomp[{i},{j}]")
            TcompPar[i,j] = model.NewIntVar(0,T,f"TcompPar[{i},{j}]")

            for r in range(R):

                A[i, j, r] = model.NewBoolVar(f"A[{i},{j},{r}]")
                interval[i,j,r] = model.NewOptionalIntervalVar(s[i,j],d[i,j],e[i,j],A[i, j, r],f"I_{i}_{j}_{r}")

                for ss in range(R):

                    if r==ss:
                        continue

                    cA[i,j,r,ss] = model.NewIntVar(0,1,f"cA[{i},{j},{r},{ss}]")

                    Ttransfer[i,j,r,ss] = model.NewIntVar(0, T,f"Ttransfer[{i},{j},{r},{ss}]")
                    TtransferLat[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    Tactive[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    
                    PxBW[i,j,r,ss] = model.NewIntVar(1,T,f"PxBW[{i},{j},{r},{ss}]")


# OBJECTIVES
    # Average slowdown
    avSlowdown = model.NewIntVar(1,T, "avSlowdown")
    sumSlowdowns = model.NewIntVar(1,T,"sumSlowdown")
    slowdowns = {}
    for j in range(J):
        
        slowdowns[j] = model.NewIntVar(1,T, f"Slowdown[{j}]")
        model.AddDivisionEquality(slowdowns[j], e[j,IT[j]-1] - l[j], sum(d[j,i] for i in range(IT[j])))
        
    model.Add(sumSlowdowns == sum(slowdowns[j] for j in range(J)))
    model.AddDivisionEquality(avSlowdown, sumSlowdowns, J)

    # makespan
    makespan = model.NewIntVar(0, T, "makespan")
    for i in range(J):
        for j in range(IT[i]):
            model.Add(makespan >= e[i,j])

    model.AddMaxEquality(makespan,[e[i, j] for i in range(J) for j in range(IT[i])])

    # turnaround time
    turnaroundTime = {}
    avTurnaroundTime = model.NewIntVar(0, T, "avTurnaround")
    sumTurnaroundTime = model.NewIntVar(0, T, "sumTurnaround")

    for j in range(J):
        turnaroundTime[j] = model.NewIntVar(0,T, f"turnaround[{j}]")
        model.Add(turnaroundTime[j] == e[j,IT[j]-1] - l[j])

    model.Add(sumTurnaroundTime == sum(turnaroundTime[j] for j in range(J)))
    model.AddDivisionEquality(avTurnaroundTime, sumTurnaroundTime, J)

    ## objective function: minimize the makespan
    #model.Minimize(makespan)

    model.Minimize(avTurnaroundTime)



    ## problem constraints
    for i in range(J):

        # Launch time
        model.Add(s[i,0] >= l[i])

        for j in range(IT[i]):

            active_pairs = []

            if j > 0:
                model.Add(s[i,j] == e[i,j-1] + 1) # an iteration cannot start until the previous iteration finishes

            # rqR = sum(A)
            model.Add(reqR[i] == sum(A[i, j, r] for r in range(R)))

            # e = s + computation
            model.Add(e[i,j] == s[i,j] + d[i,j])

            # Tcomm computation
            #model.AddDivisionEquality(MPerGPU[i,j],M[i][j],P[i,j])

            # Cpar / P
            model.AddDivisionEquality(TcompPar[i,j], Cp[i][j] + P[i,j] - 1, P[i,j])
            model.Add(Tcomp[i,j] == TcompPar[i,j] + Cs[i][j])

            for r in range(R):
                for ss in range(R):

                    if r == ss:
                        continue

                    model.AddElement(cA[i,j,r,ss],[0, TtransferLat[i,j,r,ss]],Tactive[i,j,r,ss])
                    active_pairs.append(Tactive[i,j,r,ss])

                    # P x BW
                    model.AddMultiplicationEquality(PxBW[i, j, r, ss],[P[i,j],BW[r][ss]])

                    # M / P x BW
                    model.AddDivisionEquality(Ttransfer[i,j,r,ss],M[i][j] + PxBW[i,j,r,ss] - 1,PxBW[i,j,r,ss])
                    model.Add(TtransferLat[i,j,r,ss] == Ttransfer[i,j,r,ss] + L[r][ss])

                    # cA = A x A
                    model.AddMultiplicationEquality(cA[i,j, r, ss],[A[i,j, r],A[i,j, ss]])
                    
                    # A[prev] == A[current]
                    if(j > 0):
                        model.Add(A[i,j,r] == A[i,j-1,r])

            model.AddMaxEquality(Tcomm[i,j], active_pairs)

            # d[i] computation
            model.Add(d[i,j] == Tcomp[i,j] + Tcomm[i,j])

    # intervals
    for r in range(R):
        model.AddNoOverlap([interval[i,j, r] for i in range(J) for j in range(IT[i])])


    # solve problem
    
    # solve problem
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = timeout  #* 60 * 24 # 1 day
    solver.parameters.num_search_workers = threads
    print(model.Validate())
    status = solver.Solve(model)

    # ============================================================
    # Results
    # ============================================================

    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)
    
    '''solver = cp_model.CpSolver()

    solver.parameters.max_time_in_seconds = timeout
    solver.parameters.relative_gap_limit = 0.01
    solver.parameters.num_search_workers = 16
    callback = ProgressCallback(report_interval=30.0)
    
    print(model.Validate(), flush=True)

    status = solver.SolveWithSolutionCallback(model, callback)

    #status = solver.Solve(model)

    # results
    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)


    if abs(solver.ObjectiveValue()) > 1e-9:
        gap = abs(solver.ObjectiveValue() - solver.BestObjectiveBound()) 
        max(1.0, abs(solver.ObjectiveValue()))
        print(f"Gap: {100.0 * gap:.4f}%")'''


    if status in (cp_model.OPTIMAL,cp_model.FEASIBLE):

        solution_makespan = solver.Value(makespan)
        solution_avSLD = solver.Value(avSlowdown)
        solution_avTAT = solver.Value(avTurnaroundTime)
        print(f"Makespan: {solution_makespan}",flush=True)
        print(f"BSLD: {solution_avSLD}",flush=True)
        print(f"Turnaround time: {solution_avTAT}",flush=True)

        # save solution variables in a file
        with open(resultsFile, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow(["job", "iteration","start","end","duration","resources","gpus"])

            for i in range(J):

                for j in range(IT[i]):
                    start = solver.Value(s[i,j])
                    end = solver.Value(e[i,j])
                    duration = solver.Value(d[i,j])

                    resources = P[i,j]

                    gpus = [
                        r
                        for r in range(R)
                        if solver.Value(A[i,j, r])
                    ]

                    writer.writerow([
                        i,
                        j,
                        start,
                        end,
                        duration,
                        resources,
                        ",".join(map(str, gpus))
                    ])


        # save general solver information
        solution_info = {
            "status": solver.StatusName(status),
            "makespan": solution_makespan,
            "objective": solver.ObjectiveValue(),
            "best_bound": solver.BestObjectiveBound(),
            "jobs": J,
            "resources": R,
            "time_horizon": T
        }

        with open(solutionInfoFile, "w") as f:
            json.dump(solution_info, f, indent=4)


        # ------------------------------------------------------------
        # Plot GPU allocation
        # ------------------------------------------------------------

        # Add a small margin after the makespan.
        margin = max(20, int(0.05 * solution_makespan))
        plot_end = min(T, solution_makespan + margin)

        print(
            f"Plotting time range: 0 - {plot_end}",
            flush=True
        )

        # occupancy[r, t] = iteration executing on GPU r at time t
        #
        # -1 means that the GPU is idle.
        #
        # We use a unique ID for every (job, iteration) pair.
        iteration_id = {}
        current_id = 0

        for i in range(J):
            for j in range(IT[i]):
                iteration_id[i, j] = current_id
                current_id += 1

        occupancy = -np.ones(
            (R, plot_end),
            dtype=int
        )


        # ------------------------------------------------------------
        # Fill GPU occupancy
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                # Nothing to plot if the iteration starts after
                # the plotting horizon.
                if start >= plot_end:
                    continue

                # Clip interval to plotting horizon.
                start_plot = max(0, start)
                end_plot = min(plot_end, end)

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                # Unique ID for this iteration.
                iid = iteration_id[i, j]

                for r in gpus:
                    occupancy[
                        r,
                        start_plot:end_plot
                    ] = iid


        # ------------------------------------------------------------
        # Plot
        # ------------------------------------------------------------

        fig, ax = plt.subplots(figsize=(16, 6))

        ax.imshow(
            occupancy,
            aspect="auto",
            interpolation="none",
            extent=[
                0,
                plot_end,
                R - 0.5,
                -0.5
            ]
        )

        ax.set_xlabel("Time step")
        ax.set_ylabel("GPU")

        ax.set_yticks(
            range(R)
        )

        ax.set_yticklabels(
            [f"GPU {r}" for r in range(R)]
        )

        ax.set_title(
            f"GPU Allocation "
            f"(makespan = {solution_makespan})"
        )


        # ------------------------------------------------------------
        # Vertical grid lines
        # ------------------------------------------------------------

        number_of_ticks = 20

        ax.set_xticks(
            np.linspace(
                0,
                plot_end,
                number_of_ticks + 1
            )
        )

        ax.grid(
            axis="x",
            linestyle="--",
            alpha=0.4
        )


        # ------------------------------------------------------------
        # Add iteration labels
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                if start >= plot_end:
                    continue

                end_plot = min(
                    end,
                    plot_end
                )

                visible_width = end_plot - max(start, 0)

                # Don't put labels on extremely short intervals.
                if visible_width < 3:
                    continue

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                if not gpus:
                    continue

                # Label in the middle of the iteration.
                label_x = (
                    max(start, 0) + end_plot
                ) / 2

                for r in gpus:

                    ax.text(
                        label_x,
                        r,
                        f"J{i}-I{j}",
                        ha="center",
                        va="center",
                        fontsize=8
                    )


        # ------------------------------------------------------------
        # Mark iteration boundaries
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])

                if 0 < start < plot_end:

                    ax.axvline(
                        start,
                        linestyle=":",
                        linewidth=0.8,
                        alpha=0.5
                    )


        # ------------------------------------------------------------
        # Mark makespan
        # ------------------------------------------------------------

        ax.axvline(
            solution_makespan,
            linestyle="--",
            linewidth=1.5
        )

        ax.text(
            solution_makespan,
            R - 0.5,
            f"  makespan = {solution_makespan}",
            rotation=90,
            va="bottom",
            ha="left"
        )


        # ------------------------------------------------------------
        # Save plot
        # ------------------------------------------------------------

        plt.tight_layout()

        plt.savefig(
            resultsFigure,
            dpi=300,
            bbox_inches="tight"
        )

        plt.close()

    return [status, solver]

def scheduling_MALLEABLE_problem(workloadJSON, clusterJSON, resultsFile, solutionInfoFile, resultsFigure, timeout, threads):

    # load problem data from json structures
    J = workloadJSON["nWorkflows"]
    T = 0

    # initialize arrays
    IT = [0 for j in range(J)]
    S = [0.0 for j in range(J)]
    l = [0 for j in range(J)]
    O = [0 for j in range(J)]


    C = [0 for j in range(J)]
    Cp = [0 for j in range(J)] # precompute
    Cs = [0 for j in range(J)] # precompute
    M = [0 for j in range(J)]
    minR = [0 for j in range(J)]
    maxR = [0 for j in range(J)]

    workflows = workloadJSON["workflows"]
    for j in range(J):

        # we know we are working with workflows of individual jobs
        job = workflows[j]["jobs"][0]

        l[j] = workflows[j]["l"]
        IT[j] = job["IT"]
        O[j] = job["O"]

        C[j] = [0 for i in range(IT[j])]
        Cp[j] = [0 for i in range(IT[j])]
        Cs[j] = [0 for i in range(IT[j])]
        M[j] = [0 for i in range(IT[j])]
        minR[j] = [0 for i in range(IT[j])]
        maxR[j] = [0 for i in range(IT[j])]
        S[j] = [0.0 for i in range(IT[j])]

        for i in range(IT[j]):

            C[j][i] = job["C"][i]
            M[j][i] = job["M"][i]
            minR[j][i] = job["minR"][i] # do not consider iterations by now
            maxR[j][i] = job["maxR"][i] # do not consider iterations by now
            S[j][i] = job["S"][i]


            Cp[j][i] = math.ceil(C[j][i] * (S[j][i]))
            Cs[j][i] = math.ceil(C[j][i] * (1 - S[j][i]))

            T = T + int(Cp[j][i] + Cs[j][i] + M[j][i] / 100)

    # cluster (number of GPUs)
    N = clusterJSON["N"]
    R = clusterJSON["nGPUs"]

    # load latencies and bandwidths
    L = [[0 for _ in range(R)] for _ in range(R)]
    BW = [[0 for _ in range(R)] for _ in range(R)]

    # intra node latencies and BW
    nodes = clusterJSON["nodes"]
    off = 0
    maxLatencies = [0 for _ in range(N)]
    for n in range(N):

        # get node information
        node = nodes[n]

        # loop over the GPUs in the node
        for gpu1 in range(node["nGPUs"]):
            for gpu2 in range(node["nGPUs"]):
                
                if gpu1 == gpu2:
                    continue
                    
                L[off + gpu1][off + gpu2] = node["gpusLatencies"][gpu1][gpu2]
                BW[off + gpu1][off + gpu2] = node["gpusBandwidths"][gpu1][gpu2]

                if L[off + gpu1][off + gpu2] > maxLatencies[n]:
                    maxLatencies[n] = L[off + gpu1][off + gpu2]

        off += node["nGPUs"]


    # loop over nodes and compute inter-node latencies and bandwidths
    off1 = 0
    off2 = 0
    for n1 in range(N):

        off2 = 0
        for n2 in range(N):

            node1 = nodes[n1]
            node2 = nodes[n2]

            if(n1 == n2):
                off2 = off2 + node1["nGPUs"]
                continue


            # compute latency from the GPUs in the node 1 to the node 2
            latency = maxLatencies[n1] + maxLatencies[n2] + clusterJSON["latencies"][n1][n2]
            bandwidth = clusterJSON["bandwidths"][n1][n2]
            
            # fill information in the matrix
            for gpu1 in range(node1["nGPUs"]):
                for gpu2 in range(node2["nGPUs"]):

                    L[off1 + gpu1][off2 + gpu2] = latency
                    BW[off1 + gpu1][off2 + gpu2] = bandwidth
            
            
            off2 = off2 + node2["nGPUs"]

        off1 = off1 + node1["nGPUs"]

    ## create model
    model = cp_model.CpModel()

    ## initialize decision variables
    A = {}
    cA = {}
    P = {}

    s = {}
    d = {}
    e = {}
    interval = {}

    # Tcomm computation step by step
    MPerGPU = {}
    PxBW = {}
    Ttransfer = {}
    TtransferLat = {}
    Tactive = {}
    Tcomm = {}

    # Tcomp computation step by step
    TcompPar = {}
    Tcomp = {}

    # reconf
    rf = {}
    Trf = {}
    allocationChanges = {}


    # define variables
    for i in range(J):

        for j in range(IT[i]):

            # Start/end
            s[i,j] = model.NewIntVar(0, T, f"s[{i},{j}]")
            d[i,j] = model.NewIntVar(1, T, f"d[{i},{j}]")
            e[i,j] = model.NewIntVar(1, T, f"e[{i},{j}]")

            P[i,j] = model.NewIntVar(minR[i][j], maxR[i][j], f"P[{i},{j}]")

            MPerGPU[i,j] = model.NewIntVar(0, T, f"MPerGPU[{i},{j}]")
            Tcomm[i,j] = model.NewIntVar(0,T,f"Tcomm[{i},{j}]")
            Tcomp[i,j] = model.NewIntVar(0,T,f"Tcomp[{i},{j}]")
            TcompPar[i,j] = model.NewIntVar(0,T,f"TcompPar[{i},{j}]")
            Trf[i,j] = model.NewIntVar(0,T,f"Trf[{i},{j}]")

            rf[i,j] = model.NewBoolVar(f"rf[{i},{j}]")

            for r in range(R):

                A[i, j, r] = model.NewBoolVar(f"A[{i},{j},{r}]")
                interval[i,j,r] = model.NewOptionalIntervalVar(s[i,j],d[i,j],e[i,j],A[i, j, r],f"I_{i}_{j}_{r}")

                allocationChanges[i, j, r] = model.NewIntVar(0, 1, f"allocationChange[{i},{j},{r}]")

                for ss in range(R):

                    if r==ss:
                        continue

                    cA[i,j,r,ss] = model.NewIntVar(0,1,f"cA[{i},{j},{r},{ss}]")

                    Ttransfer[i,j,r,ss] = model.NewIntVar(0, T,f"Ttransfer[{i},{j},{r},{ss}]")
                    TtransferLat[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    Tactive[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    
                    PxBW[i,j,r,ss] = model.NewIntVar(1,T,f"PxBW[{i},{j},{r},{ss}]")


    # OBJECTIVES
    # Average slowdown
    avSlowdown = model.NewIntVar(1,T, "avSlowdown")
    sumSlowdowns = model.NewIntVar(1,T,"sumSlowdown")
    slowdowns = {}
    for j in range(J):
        
        slowdowns[j] = model.NewIntVar(1,T, f"Slowdown[{j}]")
        model.AddDivisionEquality(slowdowns[j], e[j,IT[j]-1] - l[j], sum(d[j,i] for i in range(IT[j])))
        
    model.Add(sumSlowdowns == sum(slowdowns[j] for j in range(J)))
    model.AddDivisionEquality(avSlowdown, sumSlowdowns, J)

    # makespan
    makespan = model.NewIntVar(0, T, "makespan")
    for i in range(J):
        for j in range(IT[i]):
            model.Add(makespan >= e[i,j])

    model.AddMaxEquality(makespan,[e[i, j] for i in range(J) for j in range(IT[i])])

    # turnaround time
    turnaroundTime = {}
    avTurnaroundTime = model.NewIntVar(0, T, "avTurnaround")
    sumTurnaroundTime = model.NewIntVar(0, T, "sumTurnaround")

    for j in range(J):
        turnaroundTime[j] = model.NewIntVar(0,T, f"turnaround[{j}]")
        model.Add(turnaroundTime[j] == e[j,IT[j]-1] - l[j])

    model.Add(sumTurnaroundTime == sum(turnaroundTime[j] for j in range(J)))
    model.AddDivisionEquality(avTurnaroundTime, sumTurnaroundTime, J)

    ## objective function: minimize the makespan
    #model.Minimize(makespan)

    model.Minimize(avTurnaroundTime)

    ## problem constraints
    for i in range(J):

        # Launch time
        model.Add(s[i,0] >= l[i])

        for j in range(IT[i]):

            active_pairs = []
            changes = [] # changes in resources

            if j > 0:
                model.Add(s[i,j] == e[i,j-1] + 1) # an iteration cannot start until the previous iteration finishes

            # rqR = sum(A)
            model.Add(P[i,j] == sum(A[i, j, r] for r in range(R)))
            model.Add(P[i,j] >= minR[i][j])
            model.Add(P[i,j] <= maxR[i][j])

            # e = s + computation
            model.Add(e[i,j] == s[i,j] + d[i,j])

            # Tcomm computation
            #model.AddDivisionEquality(MPerGPU[i,j],M[i][j],P[i,j])

            # Cpar / P
            model.AddDivisionEquality(TcompPar[i,j], Cp[i][j] + P[i,j] - 1, P[i,j])
            model.Add(Tcomp[i,j] == TcompPar[i,j] + Cs[i][j])

            for r in range(R):

                if(j < IT[i] - 1):
                    model.AddAbsEquality(allocationChanges[i, j, r],A[i, j, r] - A[i, j + 1, r])
                changes.append(allocationChanges[i, j, r])


                for ss in range(R):

                    if r == ss:
                        continue

                    #model.AddDivisionEquality(Ttransfer[i,j,r,ss],MPerGPU[i,j],BW[r][ss])
                    #model.Add(TtransferLat[i,j,r,ss] == L[r][ss] + Ttransfer[i,j,r,ss])
                    
                    
                    #model.AddMultiplicationEquality(Tactive[i, r, ss], [cA[i, r, ss], TtransferLat[i, r, ss]])
                    #model.AddElement(cA[i,j,r,ss],[0, TtransferLat[i,j,r,ss]],Tactive[i,j,r,ss])
                    model.AddElement(cA[i,j,r,ss],[0, TtransferLat[i,j,r,ss]],Tactive[i,j,r,ss])
                    active_pairs.append(Tactive[i,j,r,ss])

                    # P x BW
                    model.AddMultiplicationEquality(PxBW[i, j, r, ss],[P[i,j],BW[r][ss]])

                    # M / P x BW
                    model.AddDivisionEquality(Ttransfer[i,j,r,ss],M[i][j] + PxBW[i,j,r,ss] - 1,PxBW[i,j,r,ss])
                    model.Add(TtransferLat[i,j,r,ss] == Ttransfer[i,j,r,ss] + L[r][ss])

                    # cA = A x A
                    model.AddMultiplicationEquality(cA[i,j, r, ss],[A[i,j, r],A[i,j, ss]])


            # rf[i,j] = 1 iff at least one GPU changes allocation
            model.AddMaxEquality(rf[i, j], changes)

                    
            model.AddMaxEquality(Tcomm[i,j], active_pairs)

            # Trf computation: Oi if rf == 1
            model.AddElement(rf[i,j],[0, O[i]],Trf[i,j])

            # d[i] computation
            model.Add(d[i,j] == Tcomp[i,j] + Tcomm[i,j] + Trf[i,j])

    # No next iteration exists after the final iteration
    model.Add(rf[i, IT[i] - 1] == 0)

    # intervals
    for r in range(R):
        model.AddNoOverlap([interval[i,j, r] for i in range(J) for j in range(IT[i])])


    # solve problem
    
    # solve problem
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = timeout  #* 60 * 24 # 1 day
    solver.parameters.num_search_workers = threads
    print(model.Validate())
    status = solver.Solve(model)

    # ============================================================
    # Results
    # ============================================================

    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)
    
    '''solver = cp_model.CpSolver()

    solver.parameters.max_time_in_seconds = timeout
    solver.parameters.relative_gap_limit = 0.01
    solver.parameters.num_search_workers = 16
    callback = ProgressCallback(report_interval=30.0)
    
    print(model.Validate(), flush=True)

    status = solver.SolveWithSolutionCallback(model, callback)

    #status = solver.Solve(model)

    # results
    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)


    if abs(solver.ObjectiveValue()) > 1e-9:
        gap = abs(solver.ObjectiveValue() - solver.BestObjectiveBound()) 
        max(1.0, abs(solver.ObjectiveValue()))
        print(f"Gap: {100.0 * gap:.4f}%")'''


    if status in (cp_model.OPTIMAL,cp_model.FEASIBLE):

        solution_makespan = solver.Value(makespan)
        solution_avSLD = solver.Value(avSlowdown)
        solution_avTAT = solver.Value(avTurnaroundTime)
        print(f"Makespan: {solution_makespan}",flush=True)
        print(f"BSLD: {solution_avSLD}",flush=True)
        print(f"Turnaround time: {solution_avTAT}",flush=True)

        # compute real values
        real_turn = 0.0
        for j in range(J):
            real_turn += float(solver.Value(e[j,IT[j]-1])) - float(solver.Value(l[j]))
        real_turn /= float(J)
        print(f"Real Turnaround time: {real_turn}",flush=True)


        # save solution variables in a file
        with open(resultsFile, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow(["job", "iteration","start","end","duration","resources","gpus"])

            for i in range(J):

                for j in range(IT[i]):
                    start = solver.Value(s[i,j])
                    end = solver.Value(e[i,j])
                    duration = solver.Value(d[i,j])

                    resources = P[i,j]

                    gpus = [
                        r
                        for r in range(R)
                        if solver.Value(A[i,j, r])
                    ]

                    writer.writerow([
                        i,
                        j,
                        start,
                        end,
                        duration,
                        resources,
                        ",".join(map(str, gpus))
                    ])


        # save general solver information
        solution_info = {
            "status": solver.StatusName(status),
            "makespan": solution_makespan,
            "objective": solver.ObjectiveValue(),
            "best_bound": solver.BestObjectiveBound(),
            "jobs": J,
            "resources": R,
            "time_horizon": T
        }

        with open(solutionInfoFile, "w") as f:
            json.dump(solution_info, f, indent=4)


        # ------------------------------------------------------------
        # Plot GPU allocation
        # ------------------------------------------------------------

        # Add a small margin after the makespan.
        margin = max(20, int(0.05 * solution_makespan))
        plot_end = min(T, solution_makespan + margin)

        print(
            f"Plotting time range: 0 - {plot_end}",
            flush=True
        )

        # occupancy[r, t] = iteration executing on GPU r at time t
        #
        # -1 means that the GPU is idle.
        #
        # We use a unique ID for every (job, iteration) pair.
        iteration_id = {}
        current_id = 0

        for i in range(J):
            for j in range(IT[i]):
                iteration_id[i, j] = current_id
                current_id += 1

        occupancy = -np.ones(
            (R, plot_end),
            dtype=int
        )


        # ------------------------------------------------------------
        # Fill GPU occupancy
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                # Nothing to plot if the iteration starts after
                # the plotting horizon.
                if start >= plot_end:
                    continue

                # Clip interval to plotting horizon.
                start_plot = max(0, start)
                end_plot = min(plot_end, end)

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                # Unique ID for this iteration.
                iid = iteration_id[i, j]

                for r in gpus:
                    occupancy[
                        r,
                        start_plot:end_plot
                    ] = iid


        # ------------------------------------------------------------
        # Plot
        # ------------------------------------------------------------

        fig, ax = plt.subplots(figsize=(16, 6))

        ax.imshow(
            occupancy,
            aspect="auto",
            interpolation="none",
            extent=[
                0,
                plot_end,
                R - 0.5,
                -0.5
            ]
        )

        ax.set_xlabel("Time step")
        ax.set_ylabel("GPU")

        ax.set_yticks(
            range(R)
        )

        ax.set_yticklabels(
            [f"GPU {r}" for r in range(R)]
        )

        ax.set_title(
            f"GPU Allocation "
            f"(makespan = {solution_makespan})"
        )


        # ------------------------------------------------------------
        # Vertical grid lines
        # ------------------------------------------------------------

        number_of_ticks = 20

        ax.set_xticks(
            np.linspace(
                0,
                plot_end,
                number_of_ticks + 1
            )
        )

        ax.grid(
            axis="x",
            linestyle="--",
            alpha=0.4
        )


        # ------------------------------------------------------------
        # Add iteration labels
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                if start >= plot_end:
                    continue

                end_plot = min(
                    end,
                    plot_end
                )

                visible_width = end_plot - max(start, 0)

                # Don't put labels on extremely short intervals.
                if visible_width < 3:
                    continue

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                if not gpus:
                    continue

                # Label in the middle of the iteration.
                label_x = (
                    max(start, 0) + end_plot
                ) / 2

                for r in gpus:

                    ax.text(
                        label_x,
                        r,
                        f"J{i}-I{j}",
                        ha="center",
                        va="center",
                        fontsize=8
                    )


        # ------------------------------------------------------------
        # Mark iteration boundaries
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])

                if 0 < start < plot_end:

                    ax.axvline(
                        start,
                        linestyle=":",
                        linewidth=0.8,
                        alpha=0.5
                    )


        # ------------------------------------------------------------
        # Mark makespan
        # ------------------------------------------------------------

        ax.axvline(
            solution_makespan,
            linestyle="--",
            linewidth=1.5
        )

        ax.text(
            solution_makespan,
            R - 0.5,
            f"  makespan = {solution_makespan}",
            rotation=90,
            va="bottom",
            ha="left"
        )


        # ------------------------------------------------------------
        # Save plot
        # ------------------------------------------------------------

        plt.tight_layout()

        plt.savefig(
            resultsFigure,
            dpi=300,
            bbox_inches="tight"
        )

        plt.close()

    return [status, solver]



def scheduling_COMPLETE_problem(workloadJSON, clusterJSON, resultsFile, solutionInfoFile, resultsFigure, timeout, threads, spec):

    # load problem data from json structures
    J = workloadJSON["nWorkflows"]
    T = 0

    # initialize arrays
    IT = [0 for j in range(J)]
    CPT = [0 for j in range(J)]
    S = [0.0 for j in range(J)]
    l = [0 for j in range(J)]
    O = [0 for j in range(J)]


    C = [0 for j in range(J)]
    SYNC = [0 for j in range(J)]
    M = [0 for j in range(J)]

    Cp = [0 for j in range(J)] # precompute
    Cs = [0 for j in range(J)] # precompute
    
    minR = [0 for j in range(J)]
    maxR = [0 for j in range(J)]
    rigidR = [0 for j in range(J)]

    mal = [0 for j in range(J)]
    mol = [0 for j in range(J)]

    workflows = workloadJSON["workflows"]
    for j in range(J):

        # we know we are working with workflows of individual jobs
        job = workflows[j]["jobs"][0]

        l[j] = workflows[j]["l"]
        IT[j] = job["IT"]
        CPT[j] = job["CPT"]
        O[j] = job["O"]
        mal[j] = job["malleable"]
        mol[j] = job["moldable"]

        # spec == 0, all jobs are rigid
        if spec == 0:
            mal[j] = 0
            mol[j] = 0

        # all jobs moldable
        if spec == 1:
            mal[j] = 0
            mol[j] = 1

        if spec == 2:
            mal[j] = 1
            mol[j] = 1

        # if soec > 2, then combined model

        C[j] = [0 for i in range(IT[j])]
        SYNC[j] = [0 for i in range(IT[j])]
        M[j] = [0 for i in range(IT[j])]
        S[j] = [0.0 for i in range(IT[j])]

        Cp[j] = [0 for i in range(IT[j])]
        Cs[j] = [0 for i in range(IT[j])]
        minR[j] = [0 for i in range(IT[j])]
        maxR[j] = [0 for i in range(IT[j])]
        rigidR[j] = [0 for i in range(IT[j])]

        for i in range(IT[j]):

            C[j][i] = job["C"][i]
            M[j][i] = job["M"][i]
            minR[j][i] = job["minR"][i] # do not consider iterations by now
            maxR[j][i] = job["maxR"][i] # do not consider iterations by now
            rigidR[j][i] = job["rigidR"][i]

            # if the job is rigid, then minR == maxR == rigidR
            if mol[j] == 0 and mal[j] == 0:
                minR[j][i] = rigidR[j][i]
                maxR[j][i] = rigidR[j][i]

            S[j][i] = job["S"][i]

            # make some precomputations
            Cp[j][i] = math.ceil(C[j][i] * (S[j][i]))
            Cs[j][i] = math.ceil(C[j][i] * (1 - S[j][i]))

            T = T + int((Cp[j][i] + Cs[j][i]) / 10 + M[j][i] / 100)


    # cluster (number of GPUs)
    N = clusterJSON["N"]
    R = clusterJSON["nGPUs"]

    # load latencies and bandwidths
    L = [[0 for _ in range(R)] for _ in range(R)]
    BW = [[0 for _ in range(R)] for _ in range(R)]

    # intra node latencies and BW
    nodes = clusterJSON["nodes"]
    off = 0
    maxLatencies = [0 for _ in range(N)]
    for n in range(N):

        # get node information
        node = nodes[n]

        # loop over the GPUs in the node
        for gpu1 in range(node["nGPUs"]):
            for gpu2 in range(node["nGPUs"]):
                
                if gpu1 == gpu2:
                    continue
                    
                L[off + gpu1][off + gpu2] = node["gpusLatencies"][gpu1][gpu2]
                BW[off + gpu1][off + gpu2] = node["gpusBandwidths"][gpu1][gpu2]

                if L[off + gpu1][off + gpu2] > maxLatencies[n]:
                    maxLatencies[n] = L[off + gpu1][off + gpu2]

        off += node["nGPUs"]


    # loop over nodes and compute inter-node latencies and bandwidths
    off1 = 0
    off2 = 0
    for n1 in range(N):

        off2 = 0
        for n2 in range(N):

            node1 = nodes[n1]
            node2 = nodes[n2]

            if(n1 == n2):
                off2 = off2 + node1["nGPUs"]
                continue


            # compute latency from the GPUs in the node 1 to the node 2
            latency = maxLatencies[n1] + maxLatencies[n2] + clusterJSON["latencies"][n1][n2]
            bandwidth = clusterJSON["bandwidths"][n1][n2]
            
            # fill information in the matrix
            for gpu1 in range(node1["nGPUs"]):
                for gpu2 in range(node2["nGPUs"]):

                    L[off1 + gpu1][off2 + gpu2] = latency
                    BW[off1 + gpu1][off2 + gpu2] = bandwidth
            
            
            off2 = off2 + node2["nGPUs"]

        off1 = off1 + node1["nGPUs"]

    ## create model
    model = cp_model.CpModel()

    ## initialize decision variables
    A = {}
    cA = {}
    P = {}

    s = {}
    d = {}
    e = {}
    interval = {}

    # Tcomm computation step by step
    MPerGPU = {}
    PxBW = {}
    Ttransfer = {}
    TtransferLat = {}
    Tactive = {}
    Tcomm = {}

    # Tcomp computation step by step
    TcompPar = {}
    Tcomp = {}
    Tsync = {}

    # reconf
    rf = {}
    Trf = {}
    allocationChanges = {}


    # define variables
    for i in range(J):

        for j in range(IT[i]):

            # Start/end
            s[i,j] = model.NewIntVar(0, T, f"s[{i},{j}]")
            d[i,j] = model.NewIntVar(1, T, f"d[{i},{j}]")
            e[i,j] = model.NewIntVar(1, T, f"e[{i},{j}]")

            P[i,j] = model.NewIntVar(minR[i][j], maxR[i][j], f"P[{i},{j}]")

            MPerGPU[i,j] = model.NewIntVar(0, T, f"MPerGPU[{i},{j}]")
            Tcomm[i,j] = model.NewIntVar(0,T,f"Tcomm[{i},{j}]")
            Tcomp[i,j] = model.NewIntVar(0,T,f"Tcomp[{i},{j}]")
            TcompPar[i,j] = model.NewIntVar(0,T,f"TcompPar[{i},{j}]")
            Tsync[i,j] = model.NewIntVar(0,T,f"Tsync[{i},{j}]")
            Trf[i,j] = model.NewIntVar(0,T,f"Trf[{i},{j}]")

            rf[i,j] = model.NewBoolVar(f"rf[{i},{j}]")

            for r in range(R):

                A[i, j, r] = model.NewBoolVar(f"A[{i},{j},{r}]")
                interval[i,j,r] = model.NewOptionalIntervalVar(s[i,j],d[i,j],e[i,j],A[i, j, r],f"I_{i}_{j}_{r}")

                allocationChanges[i, j, r] = model.NewIntVar(0, 1, f"allocationChange[{i},{j},{r}]")

                for ss in range(R):

                    if r==ss:
                        continue

                    cA[i,j,r,ss] = model.NewIntVar(0,1,f"cA[{i},{j},{r},{ss}]")

                    Ttransfer[i,j,r,ss] = model.NewIntVar(0, T,f"Ttransfer[{i},{j},{r},{ss}]")
                    TtransferLat[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    Tactive[i,j,r,ss] = model.NewIntVar(0, T,f"TtransferLat[{i},{j},{r},{ss}]")
                    
                    PxBW[i,j,r,ss] = model.NewIntVar(1,T,f"PxBW[{i},{j},{r},{ss}]")


    # OBJECTIVES
    # Average slowdown
    avSlowdown = model.NewIntVar(1,T, "avSlowdown")
    sumSlowdowns = model.NewIntVar(1,T,"sumSlowdown")
    slowdowns = {}
    for j in range(J):
        
        slowdowns[j] = model.NewIntVar(1,T, f"Slowdown[{j}]")
        model.AddDivisionEquality(slowdowns[j], e[j,IT[j]-1] - l[j], sum(d[j,i] for i in range(IT[j])))
        
    model.Add(sumSlowdowns == sum(slowdowns[j] for j in range(J)))
    model.AddDivisionEquality(avSlowdown, sumSlowdowns, J)

    # makespan
    makespan = model.NewIntVar(0, T, "makespan")
    for i in range(J):
        for j in range(IT[i]):
            model.Add(makespan >= e[i,j])

    model.AddMaxEquality(makespan,[e[i, j] for i in range(J) for j in range(IT[i])])

    # turnaround time
    turnaroundTime = {}
    avTurnaroundTime = model.NewIntVar(0, T, "avTurnaround")
    sumTurnaroundTime = model.NewIntVar(0, T, "sumTurnaround")

    for j in range(J):
        turnaroundTime[j] = model.NewIntVar(0,T, f"turnaround[{j}]")
        model.Add(turnaroundTime[j] == e[j,IT[j]-1] - l[j])

    model.Add(sumTurnaroundTime == sum(turnaroundTime[j] for j in range(J)))
    model.AddDivisionEquality(avTurnaroundTime, sumTurnaroundTime, J)

    ## objective function: minimize the makespan
    #model.Minimize(makespan)

    model.Minimize(avTurnaroundTime)

    ## problem constraints
    for i in range(J):

        # Launch time
        model.Add(s[i,0] >= l[i])

        for j in range(IT[i]):

            active_pairs = []
            changes = [] # changes in resources

            if j > 0:

                # A iteration cannot start before the previous iteration finished
                model.Add(s[i,j] == e[i,j-1] + 1)

            # P = sum(A) and minR <= P <= maxR
            model.Add(P[i,j] == sum(A[i, j, r] for r in range(R)))
            model.Add(P[i,j] >= minR[i][j])
            model.Add(P[i,j] <= maxR[i][j])

            # e = s + d, a iteration ends when all its computation has been processed
            model.Add(e[i,j] == s[i,j] + d[i,j])

            # process Cp / P -- (Cp + P - 1) / P, [ceil]
            model.AddDivisionEquality(TcompPar[i,j], Cp[i][j] + P[i,j] - 1, P[i,j])

            # synchronization cost
            model.AddMultiplicationEquality(Tsync[i, j],[P[i,j]-1,SYNC[i][j]])

            # total computation time
            model.Add(Tcomp[i,j] == TcompPar[i,j] + Cs[i][j] + Tsync[i,j])

            for r in range(R):

                if(j < IT[i] - 1):
                    model.AddAbsEquality(allocationChanges[i, j, r],A[i, j, r] - A[i, j + 1, r])
                changes.append(allocationChanges[i, j, r])

                for ss in range(R):

                    if r == ss:
                        continue

                    model.AddElement(cA[i,j,r,ss],[0, TtransferLat[i,j,r,ss]],Tactive[i,j,r,ss])
                    active_pairs.append(Tactive[i,j,r,ss])

                    # P x BW
                    model.AddMultiplicationEquality(PxBW[i, j, r, ss],[P[i,j],BW[r][ss]])

                    # M / P x BW
                    model.AddDivisionEquality(Ttransfer[i,j,r,ss],M[i][j] + PxBW[i,j,r,ss] - 1,PxBW[i,j,r,ss])
                    model.Add(TtransferLat[i,j,r,ss] == Ttransfer[i,j,r,ss] + L[r][ss])

                    # cA = A x A
                    model.AddMultiplicationEquality(cA[i,j, r, ss],[A[i,j, r],A[i,j, ss]])


            # rf can be 1 only if the job is malleable
            model.Add(rf[i,j] <= mal[i])

            # rf[i,j] = 1 iff at least one GPU changes allocation
            model.AddMaxEquality(rf[i, j], changes)

                    
            model.AddMaxEquality(Tcomm[i,j], active_pairs)

            # Trf computation: Oi if rf == 1
            model.AddElement(rf[i,j],[0, O[i]],Trf[i,j])

            # d[i] computation
            model.Add(d[i,j] == Tcomp[i,j] + Tcomm[i,j] + Trf[i,j])

    # No next iteration exists after the final iteration
    model.Add(rf[i, IT[i] - 1] == 0)


    # intervals - resource usage cannot overlap
    for r in range(R):
        model.AddNoOverlap([interval[i,j, r] for i in range(J) for j in range(IT[i])])


    # solve problem
    
    # solve problem
    solver = cp_model.CpSolver()
    solver.parameters.max_time_in_seconds = timeout  #* 60 * 24 # 1 day
    solver.parameters.num_search_workers = threads
    print(model.Validate())
    status = solver.Solve(model)

    # ============================================================
    # Results
    # ============================================================

    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)
    
    '''solver = cp_model.CpSolver()

    solver.parameters.max_time_in_seconds = timeout
    solver.parameters.relative_gap_limit = 0.01
    solver.parameters.num_search_workers = 16
    callback = ProgressCallback(report_interval=30.0)
    
    print(model.Validate(), flush=True)

    status = solver.SolveWithSolutionCallback(model, callback)

    #status = solver.Solve(model)

    # results
    print("Status:", solver.StatusName(status))
    print("Objective:", solver.ObjectiveValue())
    print("Best bound:", solver.BestObjectiveBound(), flush=True)


    if abs(solver.ObjectiveValue()) > 1e-9:
        gap = abs(solver.ObjectiveValue() - solver.BestObjectiveBound()) 
        max(1.0, abs(solver.ObjectiveValue()))
        print(f"Gap: {100.0 * gap:.4f}%")'''


    if status in (cp_model.OPTIMAL,cp_model.FEASIBLE):

        solution_makespan = solver.Value(makespan)
        solution_avSLD = solver.Value(avSlowdown)
        solution_avTAT = solver.Value(avTurnaroundTime)
        print(f"Makespan: {solution_makespan}",flush=True)
        print(f"BSLD: {solution_avSLD}",flush=True)
        print(f"Turnaround time: {solution_avTAT}",flush=True)

        # compute real values
        real_turn = 0.0
        for j in range(J):
            real_turn += float(solver.Value(e[j,IT[j]-1])) - float(solver.Value(l[j]))
        real_turn /= float(J)
        print(f"Real Turnaround time: {real_turn}",flush=True)


        # save solution variables in a file
        with open(resultsFile, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow(["job", "iteration","start","end","duration","resources","gpus"])

            for i in range(J):

                for j in range(IT[i]):
                    start = solver.Value(s[i,j])
                    end = solver.Value(e[i,j])
                    duration = solver.Value(d[i,j])

                    resources = P[i,j]

                    gpus = [
                        r
                        for r in range(R)
                        if solver.Value(A[i,j, r])
                    ]

                    writer.writerow([
                        i,
                        j,
                        start,
                        end,
                        duration,
                        resources,
                        ",".join(map(str, gpus))
                    ])


        # save general solver information
        solution_info = {
            "status": solver.StatusName(status),
            "makespan": solution_makespan,
            "objective": solver.ObjectiveValue(),
            "best_bound": solver.BestObjectiveBound(),
            "jobs": J,
            "resources": R,
            "time_horizon": T
        }

        with open(solutionInfoFile, "w") as f:
            json.dump(solution_info, f, indent=4)


        # ------------------------------------------------------------
        # Plot GPU allocation
        # ------------------------------------------------------------

        # Add a small margin after the makespan.
        margin = max(20, int(0.05 * solution_makespan))
        plot_end = min(T, solution_makespan + margin)

        print(
            f"Plotting time range: 0 - {plot_end}",
            flush=True
        )

        # occupancy[r, t] = iteration executing on GPU r at time t
        #
        # -1 means that the GPU is idle.
        #
        # We use a unique ID for every (job, iteration) pair.
        iteration_id = {}
        current_id = 0

        for i in range(J):
            for j in range(IT[i]):
                iteration_id[i, j] = current_id
                current_id += 1

        occupancy = -np.ones(
            (R, plot_end),
            dtype=int
        )


        # ------------------------------------------------------------
        # Fill GPU occupancy
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                # Nothing to plot if the iteration starts after
                # the plotting horizon.
                if start >= plot_end:
                    continue

                # Clip interval to plotting horizon.
                start_plot = max(0, start)
                end_plot = min(plot_end, end)

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                # Unique ID for this iteration.
                iid = iteration_id[i, j]

                for r in gpus:
                    occupancy[
                        r,
                        start_plot:end_plot
                    ] = iid


        # ------------------------------------------------------------
        # Plot
        # ------------------------------------------------------------

        fig, ax = plt.subplots(figsize=(16, 6))

        ax.imshow(
            occupancy,
            aspect="auto",
            interpolation="none",
            extent=[
                0,
                plot_end,
                R - 0.5,
                -0.5
            ]
        )

        ax.set_xlabel("Time step")
        ax.set_ylabel("GPU")

        ax.set_yticks(
            range(R)
        )

        ax.set_yticklabels(
            [f"GPU {r}" for r in range(R)]
        )

        ax.set_title(
            f"GPU Allocation "
            f"(makespan = {solution_makespan})"
        )


        # ------------------------------------------------------------
        # Vertical grid lines
        # ------------------------------------------------------------

        number_of_ticks = 20

        ax.set_xticks(
            np.linspace(
                0,
                plot_end,
                number_of_ticks + 1
            )
        )

        ax.grid(
            axis="x",
            linestyle="--",
            alpha=0.4
        )


        # ------------------------------------------------------------
        # Add iteration labels
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])
                end = solver.Value(e[i, j])

                if start >= plot_end:
                    continue

                end_plot = min(
                    end,
                    plot_end
                )

                visible_width = end_plot - max(start, 0)

                # Don't put labels on extremely short intervals.
                if visible_width < 3:
                    continue

                # Find GPUs assigned to this iteration.
                gpus = [
                    r
                    for r in range(R)
                    if solver.Value(A[i, j, r])
                ]

                if not gpus:
                    continue

                # Label in the middle of the iteration.
                label_x = (
                    max(start, 0) + end_plot
                ) / 2

                for r in gpus:

                    ax.text(
                        label_x,
                        r,
                        f"J{i}-I{j}",
                        ha="center",
                        va="center",
                        fontsize=8
                    )


        # ------------------------------------------------------------
        # Mark iteration boundaries
        # ------------------------------------------------------------

        for i in range(J):

            for j in range(IT[i]):

                start = solver.Value(s[i, j])

                if 0 < start < plot_end:

                    ax.axvline(
                        start,
                        linestyle=":",
                        linewidth=0.8,
                        alpha=0.5
                    )


        # ------------------------------------------------------------
        # Mark makespan
        # ------------------------------------------------------------

        ax.axvline(
            solution_makespan,
            linestyle="--",
            linewidth=1.5
        )

        ax.text(
            solution_makespan,
            R - 0.5,
            f"  makespan = {solution_makespan}",
            rotation=90,
            va="bottom",
            ha="left"
        )


        # ------------------------------------------------------------
        # Save plot
        # ------------------------------------------------------------

        plt.tight_layout()

        plt.savefig(
            resultsFigure,
            dpi=300,
            bbox_inches="tight"
        )

        plt.close()

    return [status, solver]




# main function
if __name__=="__main__":
    
    # load input arguments
    problem = int(sys.argv[1]) # problem definition (scheduling, evolving scheduling, malleable scheduling)
    inWorkload = sys.argv[2] # file path storing the workload
    inCluster = sys.argv[3] # file path storing the cluster
    resultsFile = sys.argv[4]
    solutionInfoFile = sys.argv[5]
    resultsFigure = sys.argv[6]
    timeout = int(sys.argv[7])
    threads = int(sys.argv[8])

    workload = load_workload_json(inWorkload)
    cluster = load_cluster_json(inCluster)

    if problem == 0:
        res = scheduling_ITERATIVE_RIGID_problem(workload, cluster, resultsFile, solutionInfoFile, resultsFigure, timeout, threads)
    elif problem == 1:
        res = evolving_scheduling_problem(workload, cluster, resultsFile, solutionInfoFile, resultsFigure, timeout)
    elif problem == 2:
        res = scheduling_MALLEABLE_problem(workload, cluster, resultsFile, solutionInfoFile, resultsFigure, timeout, threads)
    elif problem == 3:
        res = scheduling_COMPLETE_problem(workload, cluster, resultsFile, solutionInfoFile, resultsFigure, timeout, threads)