import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;
import java.io.*;
import java.net.*;

public class GaussImpl extends UnicastRemoteObject implements Gauss {

    private static final int NUM_PROCS = 4;
    private static final int BASE_PORT = 5000;

    public GaussImpl() throws RemoteException {
        super();
    }

    @Override
    public String sayHello(String name) throws RemoteException {
        return "Witaj, " + name + "!";
    }

    // --- helper: zamiana wierszy ---
    private void swapRow(double[][] mat, int i, int j) {
        double[] tmp = mat[i];
        mat[i] = mat[j];
        mat[j] = tmp;
    }

    // --- forward elimination ---
    private int forwardElim(double[][] mat) {
        int N = mat.length;

        for (int k = 0; k < N; k++) {
            int iMax = k;
            double vMax = Math.abs(mat[iMax][k]);

            for (int i = k + 1; i < N; i++) {
                if (Math.abs(mat[i][k]) > vMax) {
                    vMax = Math.abs(mat[i][k]);
                    iMax = i;
                }
            }

            if (Math.abs(mat[iMax][k]) < 1e-9)
                return k;

            if (iMax != k)
                swapRow(mat, k, iMax);

            for (int i = k + 1; i < N; i++) {
                double f = mat[i][k] / mat[k][k];
                for (int j = k + 1; j <= N; j++)
                    mat[i][j] -= mat[k][j] * f;
                mat[i][k] = 0;
            }
        }
        return -1;
    }

    // --- back substitution ---
    private void backSub(double[][] mat) {
        int N = mat.length;
        double[] x = new double[N];

        for (int i = N - 1; i >= 0; i--) {
            x[i] = mat[i][N];
            for (int j = i + 1; j < N; j++)
                x[i] -= mat[i][j] * x[j];
            x[i] /= mat[i][i];
        }
        //for(int i=0; i<N; i++) {
        //    System.out.printf("%.4f\n", x[i]);
        // }
    }

    @Override
    public double[][] gaussianElimination(double[][] mat) throws RemoteException {

        System.out.println("\nrozmiar: " + mat.length);
        long startWall = System.nanoTime();
        long startCpu = System.nanoTime();  // uproszczenie — w Javie dokładny CPU time wymaga MXBean

        int singular = forwardElim(mat);

        if (singular != -1) {
            System.out.println("Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n");
            return mat;
        }

        backSub(mat);

        long endWall = System.nanoTime();
        long endCpu = System.nanoTime();

        double wallSeconds = (endWall - startWall) / 1e9;
        double cpuSeconds = (endCpu - startCpu) / 1e9;

        System.out.println("szeregowo:");
        System.out.println("Czas zegarowy (wall): " + wallSeconds + " s");
        System.out.println("Czas CPU: " + cpuSeconds + " s");

        return mat;
    }

    @Override
    public double[][] gaussianEliminationProcess(double[][] mat) throws RemoteException {
        int N = mat. length;

        System.out.println("\nrozmiar: " + mat.length);
        long startWall = System.nanoTime();
        long startCpu = System.nanoTime();  // uproszczenie — w Javie dokładny CPU time wymaga MXBean

        try {
            for (int k = 0; k < N; k++) {
                // Pivoting
                int iMax = k;
                double vMax = Math.abs(mat[iMax][k]);
                for (int i = k + 1; i < N; i++) {
                    if (Math.abs(mat[i][k]) > vMax) {
                        vMax = Math.abs(mat[i][k]);
                        iMax = i;
                    }
                }

                if (Math.abs(mat[iMax][k]) < 1e-9) {
                    System.out.println("Macierz osobliwa!");
                    return mat;
                }

                if (iMax != k) {
                    double[] temp = mat[k];
                    mat[k] = mat[iMax];
                    mat[iMax] = temp;
                }

                // Podział wierszy między procesy
                int rowsPerProc = (N - (k + 1) + NUM_PROCS - 1) / NUM_PROCS;
                ServerSocket[] servers = new ServerSocket[NUM_PROCS];
                Process[] procs = new Process[NUM_PROCS];

                // Najpierw utwórz wszystkie ServerSockety
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end) continue;

                    try {
                        servers[p] = new ServerSocket(BASE_PORT + p);
                    } catch (IOException e) {
                        System.err. println("Nie udało się utworzyć ServerSocket na porcie " + (BASE_PORT + p));
                    }
                }

                // Teraz uruchom procesy worker
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || servers[p] == null) continue;

                    try {
                        //absolute path without which everything shits itself
                        String classpath = System.getProperty("java.class.path");
                        ProcessBuilder pb = new ProcessBuilder("java", "-cp", classpath, "GaussWorker",
                                String.valueOf(k), String.valueOf(start), String.valueOf(end),
                                String.valueOf(BASE_PORT + p));
                        pb. inheritIO();
                        procs[p] = pb. start();
                    } catch (IOException e) {
                        System. err.println("Nie udało się uruchomić procesu GaussWorker: " + e. getMessage());
                    }
                }

                // Komunikacja z workerami
                for (int p = 0; p < NUM_PROCS; p++) {
                    int start = k + 1 + p * rowsPerProc;
                    int end = Math.min(N, start + rowsPerProc);
                    if (start >= end || servers[p] == null) continue;

                    try (Socket socket = servers[p].accept();
                         ObjectOutputStream out = new ObjectOutputStream(socket. getOutputStream());
                         ObjectInputStream in = new ObjectInputStream(socket. getInputStream())) {

                        // Wyślij wiersz pivot
                        out.writeObject(mat[k]);
                        out.flush();

                        // Wyślij wiersze do przetworzenia
                        for (int i = start; i < end; i++) {
                            out.writeObject(mat[i]);
                            out.flush();
                        }

                        // Odbierz zaktualizowane wiersze
                        for (int i = start; i < end; i++) {
                            try {
                                double[] updatedRow = (double[]) in.readObject();
                                mat[i] = updatedRow;
                            } catch (ClassNotFoundException e) {
                                System. err.println("Błąd odczytu wiersza od workera: " + e.getMessage());
                            }
                        }
                    } catch (IOException e) {
                        System.err. println("Błąd komunikacji z workerem na porcie " + (BASE_PORT + p) + ": " + e.getMessage());
                    } finally {
                        try {
                            servers[p].close();
                        } catch (IOException e) {
                            System.err.println("Błąd zamykania ServerSocket: " + e.getMessage());
                        }
                    }
                }

                // Czekamy na zakończenie wszystkich procesów
                for (Process p : procs) {
                    if (p != null) {
                        try {
                            p.waitFor();
                        } catch (InterruptedException e) {
                            System.err.println("Proces worker został przerwany: " + e.getMessage());
                            Thread.currentThread().interrupt();
                        }
                    }
                }
            }

            // Back substitution
            double[] x = new double[N];
            for (int i = N - 1; i >= 0; i--) {
                x[i] = mat[i][N];
                for (int j = i + 1; j < N; j++)
                    x[i] -= mat[i][j] * x[j];
                x[i] /= mat[i][i];
            }

            long endWall = System.nanoTime();
            long endCpu = System.nanoTime();

            double wallSeconds = (endWall - startWall) / 1e9;
            double cpuSeconds = (endCpu - startCpu) / 1e9;

            System.out. println("procesy:");
            System.out. println("Czas zegarowy (wall): " + wallSeconds + " s");
            System.out.println("Czas CPU: " + cpuSeconds + " s");

            //System.out.println("\nRozwiązanie układu:");
            //for (int i = 0; i < N; i++)
            //   System.out.printf("x%d = %.6f\n", i + 1, x[i]);

        } catch (Exception e) {
            System.err.println("Wystąpił błąd w gaussianEliminationProcess: " + e.getMessage());
            e.printStackTrace();
        }

        return mat;
    }
}


