#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Definisi batas max data 
#define MAX_JENIS_BUKU 40
#define MAX_JUDUL_BUKU 50
#define MAX_NAMA_PENULIS 50
#define MAX_PEMINJAMAN 40
#define MAX_NAMA_PEMINJAM 50
#define MAX_USER 20
#define MAX_PASS 20
#define DENDA_PER_HARI 1000

// Status pinjam
#define STATUS_DIPINJAM 1
#define STATUS_KEMBALIKAN 0

// Struct Buku & Peminjaman
typedef struct {
    int id;
    char kode[20];
    char judul[MAX_JUDUL_BUKU];
    char penulis[MAX_NAMA_PENULIS];
    int jenis_id;
    int stok;
    int harga;
} Buku;

typedef struct {
    char nim[20];
    int id_buku;
    char nama[MAX_NAMA_PEMINJAM];
    char judul[MAX_JUDUL_BUKU];
    char tanggal_pinjam[11];   // DD-MM-YYYY
    char tanggal_kembali[11];
    int status; // 1 = dipinjam, 0 = kembali
    int denda;
} Peminjaman;

// Globals 
char jenis_list[MAX_JENIS_BUKU][30]; // daftar nama jenis
const int DEFAULT_ALLOWED_DAYS = 7;

// Utility / UI
void set_color(int warna) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)warna);
#else
    (void)warna; // noop on non-windows
#endif
}
void header_tampilan(const char *judul) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    set_color(11);
    printf("=============================================================\n");
    printf("                SISTEM PERPUSTAKAAN UKSW\n");
    printf("=============================================================\n");
    set_color(7);
    printf(">> %s\n", judul);
    printf("-------------------------------------------------------------\n");
}
void press_enter(void) {
    printf("Tekan Enter untuk lanjut...");
    int c;
    // flush leftover characters until newline 
    while ((c = getchar()) != '\n' && c != EOF) {}
}

// ---------- tanggal & denda helpers ---------- 
static int parse_date_ddmmyyyy(const char *s, struct tm *out) {
    int d, m, y;
    if (!s || sscanf(s, "%d-%d-%d", &d, &m, &y) != 3) return 0;
    memset(out, 0, sizeof(*out));
    out->tm_mday = d;
    out->tm_mon  = m - 1;
    out->tm_year = y - 1900;
    return 1;
}
static int days_between_safe(const char *start_ddmmyyyy, const char *end_ddmmyyyy) {
    struct tm t1, t2;
    if (!parse_date_ddmmyyyy(start_ddmmyyyy, &t1)) return -1;
    if (!parse_date_ddmmyyyy(end_ddmmyyyy, &t2)) return -1;
    time_t tt1 = mktime(&t1);
    time_t tt2 = mktime(&t2);
    if (tt1 == (time_t)-1 || tt2 == (time_t)-1) return -1;
    double diff = difftime(tt2, tt1);
    int days = (int)(diff / 86400.0);
    return (days >= 0) ? days : 0;
}
int hitung_denda_from_dates(const char *tgl_pinjam, const char *tgl_kembali,
                            int harga_buku, int buku_hilang, int allowed_days) {
    if (buku_hilang) return harga_buku;
    int total_days = days_between_safe(tgl_pinjam, tgl_kembali);
    if (total_days < 0) return -1; // parse gagal
    int terlambat = total_days - allowed_days;
    if (terlambat > 0) return terlambat * DENDA_PER_HARI;
    return 0;
}

// ---------- jenis list ---------- 
void init_jenis_list(void) {
    strcpy(jenis_list[0], "Umum");
    strcpy(jenis_list[1], "Fiksi");
    strcpy(jenis_list[2], "Non-Fiksi");
    strcpy(jenis_list[3], "Teknik");
    strcpy(jenis_list[4], "Komputer");
    // sisanya kosong
}
const char *get_jenis_name(int id) {
    if (id < 0 || id >= MAX_JENIS_BUKU) return "Unknown";
    return (jenis_list[id][0]) ? jenis_list[id] : "Unknown";
}

/* ---------- file helpers buku ---------- */
static int load_all_books(Buku *arr, int max) {
    FILE *f = fopen("data_buku.txt", "r");
    if (!f) return 0;
    int count = 0;
    const char *fmt = "%d|%19[^|]|%49[^|]|%49[^|]|%d|%d|%d\n";
    while (count < max && fscanf(f, fmt,
            &arr[count].id,
            arr[count].kode,
            arr[count].judul,
            arr[count].penulis,
            &arr[count].jenis_id,
            &arr[count].stok,
            &arr[count].harga) == 7) {
        count++;
    }
    fclose(f);
    return count;
}
static int save_all_books(Buku *arr, int count) {
    FILE *f = fopen("data_buku.txt", "w");
    if (!f) return 0;
    for (int i = 0; i < count; ++i) {
        fprintf(f, "%d|%s|%s|%s|%d|%d|%d\n",
                arr[i].id, arr[i].kode, arr[i].judul, arr[i].penulis,
                arr[i].jenis_id, arr[i].stok, arr[i].harga);
    }
    fclose(f);
    return 1;
}

// ---------- sample data creation ---------- 
static void ensure_sample_data(void) {
    FILE *f = fopen("data_buku.txt", "r");
    if (!f) {
        f = fopen("data_buku.txt", "w");
        if (f) {
            fprintf(f, "1|KB001|Pemrograman C Dasar|Budi Santoso|4|5|75000\n");
            fprintf(f, "2|KB002|Dasar Jaringan Komputer|Siti Aminah|3|3|85000\n");
            fprintf(f, "3|KB003|Cerita Rakyat Jawa|Agus Wijaya|1|2|40000\n");
            fclose(f);
        }
    } else fclose(f);

    FILE *p = fopen("data_peminjaman.txt", "r");
    if (!p) {
        p = fopen("data_peminjaman.txt", "w");
        if (p) {
            fprintf(p, "201901234|1|Nico|01-10-2025|08-10-2025|1|0\n");
            fclose(p);
        }
    } else fclose(p);
}

// ---------- role admin  ---------- 
//Masih pengembangan...

// ----------role peminjam (pinjam/kembali/lihat) ---------- 
void tampilkan_daftar_buku(void) {
    header_tampilan("DAFTAR BUKU");
    Buku arr[1024];
    int n = load_all_books(arr, sizeof(arr)/sizeof(arr[0]));
    if (n == 0) { printf("Belum ada data buku.\n"); press_enter(); return; }
    printf("%-4s %-4s %-8s %-30s %-15s %-6s\n","No","ID","Kode","Judul","Penulis","Stok");
    printf("----------------------------------------------------------------------------\n");
    for (int i = 0; i < n; ++i) {
        printf("%-4d %-4d %-8s %-30s %-15s %-6d\n", i+1, arr[i].id, arr[i].kode, arr[i].judul, arr[i].penulis, arr[i].stok);
    }
    printf("----------------------------------------------------------------------------\n");
    press_enter();
}

void pinjam_buku(void) {
    header_tampilan("PINJAM BUKU");
    Buku arr[1024]; int n = load_all_books(arr, sizeof(arr)/sizeof(arr[0]));
    if (n == 0) { printf("Belum ada buku untuk dipinjam.\n"); press_enter(); return; }

    Peminjaman p; char buf[128];
    printf("Masukkan NIM Anda: ");
    fgets(p.nim, sizeof(p.nim), stdin); p.nim[strcspn(p.nim, "\n")] = 0;
    printf("Masukkan ID buku yang ingin dipinjam: ");
    fgets(buf, sizeof(buf), stdin); int id_buku = (int)strtol(buf, NULL, 10);

    int idx = -1; for (int i = 0; i < n; ++i) if (arr[i].id == id_buku) { idx = i; break; }
    if (idx == -1) { printf("Buku dengan ID %d tidak ditemukan.\n", id_buku); press_enter(); return; }
    if (arr[idx].stok <= 0) { printf("Stok buku habis.\n"); press_enter(); return; }

    printf("Masukkan nama Anda: ");
    fgets(p.nama, sizeof(p.nama), stdin); p.nama[strcspn(p.nama, "\n")] = 0;

    time_t t = time(NULL); struct tm tm = *localtime(&t);
    snprintf(p.tanggal_pinjam, sizeof(p.tanggal_pinjam), "%02d-%02d-%04d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);

    printf("Masukkan tanggal kembali (DD-MM-YYYY) atau Enter untuk auto (%d hari): ", DEFAULT_ALLOWED_DAYS);
    fgets(p.tanggal_kembali, sizeof(p.tanggal_kembali), stdin); p.tanggal_kembali[strcspn(p.tanggal_kembali, "\n")] = 0;
    if (strlen(p.tanggal_kembali) == 0) {
        struct tm t2 = tm; t2.tm_mday += DEFAULT_ALLOWED_DAYS; mktime(&t2);
        snprintf(p.tanggal_kembali, sizeof(p.tanggal_kembali), "%02d-%02d-%04d", t2.tm_mday, t2.tm_mon+1, t2.tm_year+1900);
    }

    p.id_buku = id_buku; p.status = STATUS_DIPINJAM; p.denda = 0;
    strncpy(p.judul, arr[idx].judul, sizeof(p.judul)-1); p.judul[sizeof(p.judul)-1]=0;

    arr[idx].stok -= 1;
    if (!save_all_books(arr, n)) { printf("Gagal update data buku.\n"); press_enter(); return; }

    FILE *fp = fopen("data_peminjaman.txt", "a");
    if (!fp) { printf("Gagal membuka file peminjaman.\n"); press_enter(); return; }
    fprintf(fp, "%s|%d|%s|%s|%s|%d|%d\n", p.nim, p.id_buku, p.nama, p.tanggal_pinjam, p.tanggal_kembali, p.status, p.denda);
    fclose(fp);

    printf("Buku berhasil dipinjam! (ID=%d)\n", id_buku);
    press_enter();
}

void kembalikan_buku(void) {
    header_tampilan("KEMBALIKAN BUKU");
    Peminjaman arr_p[MAX_PEMINJAMAN]; int pcount = 0;
    FILE *fp = fopen("data_peminjaman.txt", "r");
    if (!fp) { printf("Belum ada data peminjaman!\n"); press_enter(); return; }
    while (pcount < MAX_PEMINJAMAN &&
           fscanf(fp, "%19[^|]|%d|%49[^|]|%10[^|]|%10[^|]|%d|%d\n",
                  arr_p[pcount].nim, &arr_p[pcount].id_buku, arr_p[pcount].nama,
                  arr_p[pcount].tanggal_pinjam, arr_p[pcount].tanggal_kembali,
                  &arr_p[pcount].status, &arr_p[pcount].denda) == 7) pcount++;
    fclose(fp);

    char buf[128];
    printf("Masukkan NIM Anda: ");
    fgets(buf, sizeof(buf), stdin); buf[strcspn(buf, "\n")] = 0;
    char nimCari[20]; strncpy(nimCari, buf, sizeof(nimCari)); nimCari[sizeof(nimCari)-1]=0;
    printf("Masukkan ID Buku yang hendak dikembalikan: ");
    fgets(buf, sizeof(buf), stdin); int idCari = (int)strtol(buf, NULL, 10);

    int found = 0;
    for (int i = 0; i < pcount; ++i) {
        if (strcmp(arr_p[i].nim, nimCari) == 0 && arr_p[i].id_buku == idCari && arr_p[i].status == STATUS_DIPINJAM) {
            found = 1;
            int hilang = 0;
            printf("Apakah buku hilang? (1 = Ya,0 = Tidak): ");
            fgets(buf, sizeof(buf), stdin); hilang = (int)strtol(buf, NULL, 10);

            Buku bks[1024]; int nb = load_all_books(bks, sizeof(bks)/sizeof(bks[0]));
            int harga_buku = 0; int idx_buku = -1;
            for (int j = 0; j < nb; ++j) if (bks[j].id == idCari) { harga_buku = bks[j].harga; idx_buku = j; break; }

            int total_denda = -1;
            int hari_between = days_between_safe(arr_p[i].tanggal_pinjam, arr_p[i].tanggal_kembali);
            if (hari_between >= 0) {
                total_denda = hitung_denda_from_dates(arr_p[i].tanggal_pinjam, arr_p[i].tanggal_kembali, harga_buku, hilang, DEFAULT_ALLOWED_DAYS);
                if (total_denda < 0) total_denda = 0;
            } else {
                int terlambat = 0;
                printf("Tanggal tidak valid. Masukkan jumlah hari keterlambatan: ");
                fgets(buf, sizeof(buf), stdin); terlambat = (int)strtol(buf, NULL, 10);
                if (hilang) total_denda = harga_buku; else total_denda = (terlambat > 0) ? terlambat * DENDA_PER_HARI : 0;
            }

            arr_p[i].denda = total_denda; arr_p[i].status = STATUS_KEMBALIKAN;
            if (!hilang && idx_buku != -1) { bks[idx_buku].stok += 1; save_all_books(bks, nb); }
            printf("\nTotal denda yang harus dibayar: Rp%d\n", total_denda);
            break;
        }
    }

    if (!found) { printf("Data peminjaman tidak ditemukan atau sudah dikembalikan.\n"); press_enter(); return; }

    FILE *fw = fopen("data_peminjaman.txt", "w");
    if (!fw) { printf("Gagal memperbarui file peminjaman!\n"); press_enter(); return; }
    for (int k = 0; k < pcount; ++k) {
        fprintf(fw, "%s|%d|%s|%s|%s|%d|%d\n",
                arr_p[k].nim, arr_p[k].id_buku, arr_p[k].nama,
                arr_p[k].tanggal_pinjam, arr_p[k].tanggal_kembali,
                arr_p[k].status, arr_p[k].denda);
    }
    fclose(fw);
    printf("Buku berhasil dikembalikan dan data telah diperbarui.\n");
    press_enter();
}

void lihat_peminjaman(void) {
    header_tampilan("RIWAYAT PEMINJAMAN");
    FILE *f = fopen("data_peminjaman.txt", "r");
    if (!f) { printf("Belum ada data peminjaman.\n"); press_enter(); return; }
    Peminjaman p;
    printf("%-12s %-6s %-20s %-12s %-12s %-8s %-8s\n", "NIM", "ID", "Nama", "Pinjam", "Kembali", "Status", "Denda");
    printf("----------------------------------------------------------------------------\n");
    while (fscanf(f, "%19[^|]|%d|%49[^|]|%10[^|]|%10[^|]|%d|%d\n",
                  p.nim, &p.id_buku, p.nama, p.tanggal_pinjam, p.tanggal_kembali, &p.status, &p.denda) == 7) {
        printf("%-12s %-6d %-20s %-12s %-12s %-8s Rp%d\n",
               p.nim, p.id_buku, p.nama, p.tanggal_pinjam, p.tanggal_kembali,
               (p.status==STATUS_DIPINJAM?"Dipinjam":"Kembali"), p.denda);
    }
    fclose(f); press_enter();
}

// ---------- menus & login ---------- 
/*int login_admin(void) {
    char username[32], password[32];
    const char userAdmin[] = "admin";
    const char passAdmin[] = "admin1234";

    header_tampilan("LOGIN ADMIN");
    printf("Username: "); fgets(username, sizeof(username), stdin); username[strcspn(username, "\n")] = 0;
    printf("Password: "); fgets(password, sizeof(password), stdin); password[strcspn(password, "\n")] = 0;

    if (strcmp(username, userAdmin) == 0 && strcmp(password, passAdmin) == 0) { printf("\nLogin berhasil! Selamat datang, %s.\n", username); press_enter(); return 1; }
    else { printf("\nUsername atau password salah!\n"); press_enter(); return 0; }
} */
int login_peminjam(void) { header_tampilan("LOGIN PEMINJAM (Sederhana)"); printf("Tekan Enter untuk lanjut ke menu peminjam..."); press_enter(); return 1; }

/*void menu_admin(void) {
    int pilihan = 0;
    do {
        header_tampilan("MENU ADMIN");
        printf("1. Tambah Buku\n2. Lihat Daftar Buku\n3. Hapus Buku\n4. Lihat Data Peminjaman\n5. Kembali ke Menu Utama\n");
        printf("Pilih menu: ");
        char buf[16]; fgets(buf, sizeof(buf), stdin); pilihan = (int)strtol(buf, NULL, 10);
        switch (pilihan) {
            case 1: tambah_buku(); break;
            case 2: lihat_buku(); break;
            case 3: hapus_buku(); break;
            case 4: lihat_peminjaman_admin(); break;
            case 5: break;
            default: printf("Pilihan tidak valid!\n"); press_enter(); break;
        }
    } while (pilihan != 5);
} */

void menu_peminjam(void) {
    int pilih = 0;
    do {
        header_tampilan("MENU PEMINJAM");
        printf("1. Lihat Daftar Buku\n2. Pinjam Buku\n3. Kembalikan Buku\n4. Lihat Riwayat Peminjaman\n5. Logout\n");
        printf("Pilih menu (1-5): ");
        char buf[16]; fgets(buf, sizeof(buf), stdin); pilih = (int)strtol(buf, NULL, 10);
        switch (pilih) {
            case 1: tampilkan_daftar_buku(); break;
            case 2: pinjam_buku(); break;
            case 3: kembalikan_buku(); break;
            case 4: lihat_peminjaman(); break;
            case 5: printf("Logout...\n"); press_enter(); break;
            default: printf("Pilihan tidak valid!\n"); press_enter(); break;
        }
    } while (pilih != 5);
}

// ---------- main ---------- 
int main(void) {
    init_jenis_list();
    ensure_sample_data(); // membuat file contoh jika belum ada
    int pilih = 0;
    do {
        header_tampilan("SISTEM PERPUSTAKAAN");
        printf("1. Login sebagai Admin\n2. Login sebagai Peminjam Buku\n3. Keluar Program\n");
        printf("Pilih menu: ");
        char buf[16]; fgets(buf, sizeof(buf), stdin); pilih = (int)strtol(buf, NULL, 10);
        switch (pilih) {
            case 1: printf("\n⚙️  Maaf, fitur Admin sedang dalam tahap pengembangan.\n"); press_enter(); break; /* if (login_admin()) menu_admin(); break; */
            case 2: if (login_peminjam()) menu_peminjam(); break;
            case 3: printf("Terima kasih telah menggunakan sistem perpustakaan ini.\n"); break;
            default: printf("Pilihan tidak valid!\n"); press_enter(); break;
        }
    } while (pilih != 3);
    return 0;
}
