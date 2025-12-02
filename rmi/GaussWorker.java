import java.io.*;
import java. net.Socket;

public class GaussWorker {
    public static void main(String[] args) {
        if (args.length != 4) {
            System.err. println("Usage: java GaussWorker <k> <start> <end> <port>");
            System.exit(1);
        }

        try {
            int k = Integer.parseInt(args[0]);
            int start = Integer.parseInt(args[1]);
            int end = Integer.parseInt(args[2]);
            int port = Integer.parseInt(args[3]);

            // Połączenie z procesem macierzystym
            try (Socket socket = new Socket("localhost", port);
                 ObjectInputStream in = new ObjectInputStream(socket. getInputStream());
                 ObjectOutputStream out = new ObjectOutputStream(socket.getOutputStream())) {

                // Odbierz wiersz pivot (wiersz k)
                double[] pivotRow = (double[]) in.readObject();

                // Odbierz wiersze do przetworzenia
                double[][] rows = new double[end - start][];
                for (int i = 0; i < end - start; i++) {
                    rows[i] = (double[]) in.readObject();
                }

                // Wykonaj eliminację Gaussa dla przypisanych wierszy
                for (int i = 0; i < rows.length; i++) {
                    double[] row = rows[i];
                    double factor = row[k] / pivotRow[k];

                    for (int j = k; j < row.length; j++) {
                        row[j] -= factor * pivotRow[j];
                    }

                    // Wyślij zaktualizowany wiersz z powrotem
                    out. writeObject(row);
                    out.flush();
                }

            } catch (ClassNotFoundException e) {
                System. err.println("Błąd deserializacji danych: " + e.getMessage());
                System.exit(1);
            }

        } catch (NumberFormatException e) {
            System.err.println("Nieprawidłowe argumenty liczbowe: " + e.getMessage());
            System.exit(1);
        } catch (IOException e) {
            System.err. println("Błąd komunikacji z procesem macierzystym: " + e.getMessage());
            System. exit(1);
        }
    }
}