#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>
#include <vector>
#include <queue>
#include <memory>
#include <fstream>
#include <map>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <chrono>
#include <string>
#include <climits>
#include <cstdlib>

/*
    Bu program, sanal bir ekosistemde hayvanlarý (memeliler, bitkiler) simüle etmektedir.
    - Hayvanlarýn çeþitli özellikleri (hýz, gizlilik, tespit becerisi, vb.), yaþlanma, üreme, açlýk ve saðlýk gibi etkenler dinamik olarak güncellenir.
    - Bitkiler ise, belirli bir "food" deðerine (yenilebilir kýsým) sahiptir ve zamanla yeniden büyür.
    - QuadTree veri yapýsý, alandaki hayvanlarý ve bitkileri bölgesel olarak saklamak, böylece hýzlý arama (yakýnda kim var?) yapabilmek için kullanýlýr.
    - Kod bir Environment (çevre) yaratýr, içinde hayvan ve bitkileri ekler ve belli adým sayýsý boyunca simülasyonu ileri sarar.
    - Adýmlar boyunca alýnan veriler (bitkilerin durumu, hayvanlarýn pozisyonlarý, saðlýk, açlýk vb.) JSON dosyalara yazýlýr.
    - Simülasyon sonunda, bir Python betiðiyle (simulation8.py) verileri iþlemek veya görselleþtirmek mümkün olabilir.

    Bu kodu daha iyi uygulama pratikleri (good code practice) ile açýklamaya çalýþtýk ve mevcut/comment satýrlarý Türkçe olarak yeniden yazdýk.
*/

// Standart isim alanlarýný olabildiðince kýsa kullanýyoruz.
using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

/*
    basePath, tüm verilerin (JSON dosyalarýnýn) kaydedileceði temel (base) dizin yolunu belirtir.
*/
string basePath = "C:\\Users\\Doruk\\env\\tubitak2025\\json\\1\\";

/*
    Simülasyon sýrasýnda kullanacaðýmýz sabitler ve global deðiþkenler.
    - PI: 3.141592 deðerinde sabit.
    - NUM_ANIMALS: Toplam 10 hayvan türü.
*/
const double PI = 3.141592;
const int NUM_ANIMALS = 10;

/*
    Bir hayvanýn özelliklerini tutmak için kullanýlan yapý (AnimalTemplate).
    Her tür için;
     - speed, stealthLevel, detectionSkill, detectionRange, healthFactor: Çoklayýcý (çarpan) deðerleri.
     - reproductionCooldown: Üreme sonrasýnda bekleme süresi.
     - deathTime: Yaþam süresi (ölüm zamaný).
     - foodCapacity: Yediðinde hayvana saðlayacaðý gýda miktarý (kg).
*/
struct AnimalTemplate {
    double speed;
    double stealthLevel;
    double detectionSkill;
    double detectionRange;
    double healthFactor;
    int reproductionCooldown;
    int deathTime;
    double foodCapacity;
};

/*
    Simülasyonda rastgelelik eklemek için, üreme zamanlarýný (cooldown) ve
    ölüm sürelerini (deathTime) hayvan türüne göre saklýyoruz.
    Ayrýca bir hayvanýn bir defada doðurabileceði maksimum yavru sayýsý (maxBirthNum) da burada.
*/
int reproductionCooldownRandom[NUM_ANIMALS] = { 300, 300, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000 };
int deathTimeRandom[NUM_ANIMALS] = { 800, 800, 3300, 3300, 3300, 3300, 3300, 3300, 3300, 3300 };
int maxBirthNum[NUM_ANIMALS] = { 1,   1,   1,    1,    1,    1,    1,    1,    1,    1 };

/*
    Herbivor (otçul) hayvanlar için global bir hýz buff'ý (þu an 0).
    food_rej_per_step, her adýmda bitkilerin kendini yenileme miktarý (0.1).
*/
double buff_herbivor_speed = 0;
double food_rej_per_step = 0.05; // Her adýmda bitkilerin artan 'food' miktarý.

/*
    animalTemplates haritasýnda, tür numarasýna göre hayvan özellik çarpanlarý saklanmaktadýr.
    Örnek: {0, {1.7, 1.8, 1.5, 1, 0.5, 1700, 8000, 105}} -> Tavþan (Rabbit)
*/
std::map<int, AnimalTemplate> animalTemplates = {
    // Otçullar (Herbivores)
    {0, {1.7, 1.8, 1.5, 1, 0.5, 1700, 8000, 105}},
    {1, {1.7, 1.8, 1.5, 1, 0.5, 1700, 8000, 105}}, 
    {2, {1.3, 1.6, 1.1, 1, 0.6, 4000,15000, 5}},
    {3, {1.2, 1.4, 1.2, 1, 0.9, 4000,15000, 50.0}},

    // Hepçiller (Omnivores)
    {4, {1.2, 1.7, 1.4, 1, 0.7, 4000,15000, 5.0}},
    {5, {1.32, 1.3, 1.5, 1, 0.9, 4000,15000, 55.0}},

    // Etçiller (Carnivores)
    {6, {1.7, 1.9, 1,   1, 0.7, 3500, 15000, 35.0}},
    {7, {1.25,1.6, 1,   1, 0.8, 4000, 15000, 55.0}},
    {8, {1.2, 1.2, 1,   1, 1.0, 4000, 15000, 60.0}},
    {9, {1.3, 1.8, 1,   1, 0.8, 4000, 15000, 20.0}}
};

/*
    AnimalLimitMax ve AnimalLimitMin, genetik varyasyon sýrasýnda özelliklerin
    aþýrý yükselmesini veya düþmesini sýnýrlamak adýna belirlendi.
    (Bu örnekte hepsi a=10 ve b=0 ile tutuluyor, yani çok kýsýtlayýcý deðil)
*/
double a = 10.0;
std::map<int, AnimalTemplate> animalLimitMax = {
    // Otçullar
    {0, {a, a, a, a, a, 250}}, // Rabbit
    {1, {a, a, a, a, a, 250}}, // Deer
    {2, {a, a, a, a, a, 250}}, // Squirrel
    {3, {a, a, a, a, a, 250}}, // Elk
    // Hepçiller
    {4, {a, a, a, a, a, 250}}, // Raccoon
    {5, {a, a, a, a, a, 250}}, // Wild Pig
    // Etçiller
    {6, {a, a, a, a, a, 250}}, // Fox
    {7, {a, a, a, a, a, 250}}, // Wolf
    {8, {a, a, a, a, a, 250}}, // Bear
    {9, {a, a, a, a, a, 250}}  // Lynx
};

double b = 0;
std::map<int, AnimalTemplate> animalLimitMin = {
    // Otçullar
    {0, {b, b, b, b, b, 250}}, // Rabbit
    {1, {b, b, b, b, b, 250}}, // Deer
    {2, {b, b, b, b, b, 250}}, // Squirrel
    {3, {b, b, b, b, b, 250}}, // Elk
    // Hepçiller
    {4, {b, b, b, b, b, 250}}, // Raccoon
    {5, {b, b, b, b, b, 250}}, // Wild Pig
    // Etçiller
    {6, {b, b, b, b, b, 250}}, // Fox
    {7, {b, b, b, b, b, 250}}, // Wolf
    {8, {b, b, b, b, b, 250}}, // Bear
    {9, {b, b, b, b, b, 250}}  // Lynx
};

/*
    Tüm hayvan isimlerinin string gösterimi, index ile eþleþecek þekilde.
*/
std::string animalNames[NUM_ANIMALS] = {
    "Rabbit", "Deer", "Squirrel", "Elk",
    "Raccoon", "Wild Pig",
    "Fox", "Wolf", "Bear", "Lynx"
};

/*
    animalSpeciesWeights, hangi türden ne kadar oluþturacaðýmýzýn aðýrlýk daðýlýmý (distribution).
    Örneðin [150, 150, 0, 0, 0, 0, 0, 0, 0, 0] -> ilk iki türe (Rabbit, Deer) aðýrlýk verdiðimiz anlamýna gelir.
*/
std::vector<int> animalSpeciesWeights = {
    150, 150, 0, 0,
    0, 0,
    0, 0, 0, 0
};

/*
    base_health_decay_rate_arr ve aging_factor_arr, yaþlanma ve saðlýk bozulmasý için kullanýlýyor.
    Bu dizi hayvanýn tür indexine göre ayarlanýr.
*/
double r = 0;
double base_health_decay_rate_arr[NUM_ANIMALS] = { r, r, r, r, r, r, r, r, r, r };
double k = 0;
double aging_factor_arr[NUM_ANIMALS] = { k, k, k, k, k, k, k, k, k, k };

/*
    foodChainMatrix, av-avcý iliþkisini gösteren bir matristir.
    [satýr = Avcý (predator), sütun = Av (prey)].
    Örneðin: foodChainMatrix[6][0] = 1 demek, Fox (6) tavþaný (0) yiyebilir.
    Son sütun (NUM_ANIMALS deðeri) 'Herbivore' etiketini temsil eder (bitkisel besin).
*/
int foodChainMatrix[NUM_ANIMALS][NUM_ANIMALS + 1] = {
    //  Rabbit, Deer, Squirrel, Elk, Raccoon, Wild Pig, Fox, Wolf, Bear, Lynx, Herbivore
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Rabbit    
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Deer
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Squirrel
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Elk
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Raccoon
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, // Wild Pig
    { 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0 }, // Fox
    { 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0 }, // Wolf
    { 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0 }, // Bear
    { 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 }  // Lynx
};

/*
    Binom daðýlým (binomial distribution) fonksiyonu.
    n deneme içinde p olasýlýkla baþarýlý olma sayýsýný rastgele döndürür.
*/
int bin_dist(int n, double p) {
    static std::mt19937 mt(std::time(nullptr));
    std::binomial_distribution<int> dist(n, p);
    return dist(mt);
}

/*
    lineCircleIntersection, bir çizginin (A_x,A_y) ile (B_x,B_y) arasýndaki kesitinin,
    bir dairenin (merkez C_x,C_y ve yarýçap R) içinde veya kesiþiminde olup olmadýðýný döndürür.
*/
bool lineCircleIntersection(double A_x, double A_y, double B_x, double B_y,
    double C_x, double C_y, double R)
{
    double Ax = A_x - C_x;
    double Ay = A_y - C_y;
    double Bx = B_x - C_x;
    double By = B_y - C_y;

    if (hypot(Ax, Ay) <= R && hypot(Bx, By) <= R) {
        return true;
    }

    double a = Ax * Ax + Ay * Ay - R * R;
    double b = 2 * (Ax * (Bx - Ax) + Ay * (By - Ay));
    double c = (Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay);

    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return false;
    }
    else if (discriminant == 0) {
        double t = -b / (2 * a);
        if (t >= 0 && t <= 1) {
            return false;
        }
    }
    else {
        double sqrt_discriminant = sqrt(discriminant);
        double t1 = (-b + sqrt_discriminant) / (2 * a);
        double t2 = (-b - sqrt_discriminant) / (2 * a);

        if (t1 >= 0 && t1 <= 1) {
            return true;
        }
        if (t2 >= 0 && t2 <= 1) {
            return true;
        }
    }
    return false;
}

/*
    Ýleri deklarasyonlar (forward declarations):
    Kodun akýþýnda önce kullanýlýp sonra tanýmlanan sýnýflar.
*/
class QuadTree;
class Entity;
class Animal;
class Plant;
class BirthQueue;

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    Entity sýnýfý, x ve y koordinatý, boyutu (size) ve tipi (type) olan
    temel (base) bir varlýk sýnýfýdýr.
    Bitki (Plant) ve Hayvan (Animal) bu sýnýftan türeyebilir.
*/
class Entity {
protected:
    double x_coordinate;
    double y_coordinate;
    double size;
    int type;

public:
    // Kurucu fonksiyon (constructor)
    Entity(double x, double y, double s, int t)
        : x_coordinate(x), y_coordinate(y), size(s), type(t) {}

    virtual ~Entity() {};

    double getX() const { return x_coordinate; }
    double getY() const { return y_coordinate; }
    double getSize() const { return size; }
};

/*
    Plant (Bitki) sýnýfý, Entity sýnýfýndan türemiþtir.
    maxFood: bitkinin maksimum gýda potansiyeli (tamamen büyümüþ hâlde).
    food: o anki mevcut gýda miktarý.
*/
class Plant : public Entity {
protected:
    double maxFood;
    double food;

public:
    Plant(double x, double y, double s, double f)
        : Entity(x, y, s, 0), maxFood(f), food(0) {}

    double getFood() const { return food; }
    double getMaxFood() const { return maxFood; }
    void setFood(double f) { food = f; }
};

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    BirthQueue (Doðum Kuyruðu), üremeyle doðacak yeni hayvanlarýn verilerini saklar.
    Ýþlem sýrasý ile (FIFO) yönetilir.
*/
class BirthQueue {
public:
    // Bu yapý, yeni doðacak hayvanýn özelliklerini tutar.
    struct BirthInfo {
        int species;
        double x, y, speed, detectionRange, stealthLevel, detectionSkill;
    };

    std::queue<BirthInfo> birthQueue;

    // Kuyruða yeni hayvan ekleme
    void enqueueBirth(int species, double x, double y, double speed,
        double detectionRange, double stealthLevel, double detectionSkill)
    {
        birthQueue.push({ species, x, y, speed, detectionRange, stealthLevel, detectionSkill });
    }

    // Kuyrukta doðum bekleyen var mý?
    bool hasPendingBirths() const {
        return !birthQueue.empty();
    }

    // Kuyruktan en eski eklenen doðumu çek
    BirthInfo dequeueBirth() {
        if (birthQueue.empty()) {
            throw std::runtime_error("Bos dogum (Birth) kuyruðundan dequeue yapilamaz.");
        }
        BirthInfo birthInfo = birthQueue.front();
        birthQueue.pop();
        return birthInfo;
    }
};

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    Animal (Hayvan) sýnýfý, çevre içinde hareket edebilen, üreme ve yeme fonksiyonlarýna sahip bir varlýktýr.
    Özellikleri:
     - x,y: Konum.
     - angle: Hareket yönü (radyan cinsinden).
     - speed_coefficient: Temel hýz deðeri.
     - detection_range: Algýlama menzili.
     - stealth_level: Gizlilik deðeri.
     - detection_skill: Algýlama yeteneði.
     - hunger, health, age vb. hayvanýn durumsal deðiþkenleri.
     - birthQueuePtr: Doðum iþlemlerini takip eden kuyrukla etkileþim (yeni hayvan eklenmesi vs.)
*/
class Animal {
protected:
    int id;
    double x_coordinate;
    double y_coordinate;
    double angle;
    double last_change;
    double speed_coefficient;
    double current_speed;
    double detection_range;
    double detection_skill;
    std::vector<Animal*>* animalsPtr;
    double maxHunger;
    double hunger;
    double maxHealth;
    double health;
    int state;
    int age;
    long long death_time;
    double max_turn_rate;
    int species;
    int reproduction_cooldown;
    bool is_ready_to_reproduce;
    bool male;
    bool isPregnant;
    bool statsReduced;            // Gebelik sýrasýnda yetenekler azaltýldý mý?

    // Gebelik (rahim) verileri
    vector<double> Womb;

    BirthQueue* birthQueuePtr;

    double stealth_level;
    double current_stealth;
    double aging_factor;
    double base_health_decay_rate;
    Animal* currentTarget;

public:
    /*
        Animal kurucusu (constructor). Parametreler:
         - id_: Hayvan kimliði
         - x,y: Baþlangýç konumu
         - speed: Hýz çarpaný
         - detectionRange: Algýlama mesafesi
         - species_: Tür kimliði
         - stealth: Gizlilik seviyesi
         - detection: Algýlama becerisi
         - animalsPtr_: Hayvanlarýn saklandýðý vektörün iþaretçisi
         - birthQueuePtr_: Doðum kuyruðu iþaretçisi
    */
    Animal(int id_, double x, double y, double speed, double detectionRange, int species_,
        double stealth, double detection,
        std::vector<Animal*>* animalsPtr_, BirthQueue* birthQueuePtr_)
        : id(id_),
        x_coordinate(x),
        y_coordinate(y),
        angle(0),
        last_change(0),
        speed_coefficient(speed),
        current_speed(speed_coefficient),
        detection_range(detectionRange),
        detection_skill(detection),
        animalsPtr(animalsPtr_),
        maxHunger(100),
        hunger(maxHunger* (0.2 + ((std::mt19937{ std::random_device{}() }() % 100) / 100) * 0.6)),
        maxHealth(100 + (std::mt19937{ std::random_device{}() }() % 50)),
        health(maxHealth),
        state(Idle),
        age(0),
        death_time(animalTemplates[species_].deathTime + (std::mt19937{ std::random_device{}() }() % deathTimeRandom[species_])),
        max_turn_rate(PI / 4),
        species(species_),
        reproduction_cooldown(animalTemplates[species_].reproductionCooldown + (std::mt19937{ std::random_device{}() }() % reproductionCooldownRandom[species_])),
        is_ready_to_reproduce(false),
        male(rand() % 2 == 0),
        isPregnant(false),
        statsReduced(false),
        birthQueuePtr(birthQueuePtr_),
        stealth_level(stealth),
        current_stealth(stealth),
        aging_factor(aging_factor_arr[species_]),
        base_health_decay_rate(base_health_decay_rate_arr[species_]),
        currentTarget(nullptr)
    {}

    /*
        TÜRKÇE:
        applyAging(), hayvanýn yaþlanmasýný simüle eder.
        Zamanla hýz, gizlilik, algýlama vb. deðerleri düþer.
        Ayný zamanda maksimum saðlýðý düþebilir.
    */
    void applyAging() {
        maxHealth -= base_health_decay_rate;
        health = min(health, maxHealth);

        speed_coefficient -= speed_coefficient * aging_factor;
        detection_range -= detection_range * aging_factor;
        stealth_level -= stealth_level * aging_factor;
        detection_skill -= detection_skill * aging_factor;

        speed_coefficient = std::max(0.0, speed_coefficient);
        detection_range = std::max(0.0, detection_range);
        stealth_level = std::max(0.0, stealth_level);
        detection_skill = std::max(0.0, detection_skill);

        if (maxHealth <= 0) {
            health = 0;
        }
    }

    // Hayvanýn o an algýladýðý diðer hayvanlar/entiteler/bitkiler
    vector<Animal*> detectedAnimals;
    vector<Entity*> detectedEntities;
    vector<Plant*> detectedPlants;

    // Getter-Setter metodlarý
    double getX() const { return x_coordinate; }
    double getY() const { return y_coordinate; }
    void setX(double x) { x_coordinate = x; }
    void setY(double y) { y_coordinate = y; }
    double getAngle() const { return angle; }
    double getHealth() const { return health; }
    void setHealth(double h) { health = h; }
    double getHunger() const { return hunger; }
    int getState() const { return state; }
    double getRange() const { return detection_range; }
    int getId() const { return id; }
    double getSpeed() const { return current_speed; }
    int getSpecies() const { return species; }
    double getStealthLevel() const { return stealth_level; }
    double getDetectionSkill() const { return detection_skill; }
    void setAngle(double ang) { angle = ang; }
    double getFoodCapacity() const { return animalTemplates[species].foodCapacity; }
    double getCurrentStealth() const { return current_stealth; }
    bool isMale() const { return male; }
    Animal* getTarget() const { return currentTarget; }

    /*
        Bir hayvanýn diðeriyle çiftleþebilmesi için;
         - Ayný tür olmalý
         - Biri erkek, diðeri diþi olmalý
         - Ýkisi de hamile olmamalý (gebelik durumu yok)
    */
    bool canMateWith(const Animal* other) const {
        return (species == other->species)
            && (male != other->male)
            && (!isPregnant)
            && (!other->isPregnant);
    }

    /*
        updatePregnancy(), hayvan hamileyse gebelik sürecini yürütür,
        süre sonunda ise yavrularýný doðurur (birthQueue'ya ekler).
    */
    void updatePregnancy() {
        if (isPregnant) {
            if (!statsReduced) {
                // Gebeliðin baþlangýcýnda hayvanýn bazý deðerlerini düþür
                speed_coefficient *= 0.8;
                detection_range *= 0.8;
                stealth_level *= 0.8;
                detection_skill *= 0.8;
                statsReduced = true;
            }

            // reproduction_cooldown sýfýra inince doðum gerçekleþiyor.
            if (reproduction_cooldown <= 0) {
                // Gebelik bittiðinde deðerleri geri yükle
                speed_coefficient /= 0.8;
                detection_range /= 0.8;
                stealth_level /= 0.8;
                detection_skill /= 0.8;

                // Womb içinde yavrularýn özellikleri saklý, oradan alýnýp doðum kuyruðuna ekleniyor.
                int modulo = std::mt19937{ std::random_device{}() }() % maxBirthNum[species];
                for (int i = 0; i < 1 + modulo; i++) {
                    birthQueuePtr->enqueueBirth(
                        species,
                        Womb[0], Womb[1],
                        Womb[2], Womb[3],
                        Womb[4], Womb[5]
                    );
                }

                cout << "Hayvan ID: " << getId() << " (tur: " << animalNames[species] << ") basarili sekilde dogum yapti.\n";

                isPregnant = false;
                reproduction_cooldown = 0;
                statsReduced = false;
            }
        }
    }

    // Hayvaný hareket ettiren basit fonksiyonlar
    void moveForward() {
        x_coordinate += cos(angle) * current_speed;
        y_coordinate += sin(angle) * current_speed;
    }

    /*
        turnRandomly() hayvanýn bakýþ açýsýný (angle) rastgele deðiþtirir.
        last_change, bu deðiþikliði hatýrlamak için tutulur.
    */
    void turnRandomly() {
        double p = (last_change > 0) ? 0.7 : (last_change < 0) ? 0.3 : 0.5;
        double change = (bin_dist(10, p) - 5) / 180.0 * PI;

        if (change > max_turn_rate)  change = max_turn_rate;
        if (change < -max_turn_rate) change = -max_turn_rate;

        angle += change;
        last_change = change;

        while (angle < 0)     angle += 2 * PI;
        while (angle >= 2 * PI) angle -= 2 * PI;
    }

    /*
        turn(double ang), hayvanýn açý farkýný hesaplayarak
        yavaþça (max_turn_rate'i aþmayacak þekilde) o yöne dönmesini saðlar.
    */
    void turn(double ang) {
        double diff = ang - angle;

        while (diff < -PI) diff += 2 * PI;
        while (diff > PI)  diff -= 2 * PI;

        if (fabs(diff) > max_turn_rate) {
            angle += (diff > 0 ? max_turn_rate : -max_turn_rate);
        }
        else {
            angle = ang;
        }

        while (angle < 0)       angle += 2 * PI;
        while (angle >= 2 * PI) angle -= 2 * PI;
    }

    /*
        moveTowards(), hayvanýn parametre olarak verilen (target_x, target_y) konumuna doðru
        yavaþça dönerek ilerlemesini saðlar.
    */
    void moveTowards(double target_x, double target_y) {
        double dx = target_x - x_coordinate;
        double dy = target_y - y_coordinate;
        double target_angle = atan2(dy, dx);

        turn(target_angle);
        moveForward();
    }

    /*
        getDistance(), (target_x, target_y) noktasýna olan uzaklýðý döndürür.
    */
    double getDistance(double target_x, double target_y) const {
        double dx = target_x - x_coordinate;
        double dy = target_y - y_coordinate;
        return hypot(dx, dy);
    }

    /*
        moveRandomly(), hayvanýn rastgele yönlendirilmesini ve ileri hareket etmesini saðlar.
    */
    void moveRandomly() {
        turnRandomly();
        moveForward();
    }

    // Algýlama (Detection) fonksiyonlarý
    bool isInDetectionZone(double x, double y) const {
        double dx = x - x_coordinate;
        double dy = y - y_coordinate;
        double distanceSquared = dx * dx + dy * dy;
        return distanceSquared < (detection_range * detection_range);
    }

    void addDetectedAnimal(Animal* animal) {
        detectedAnimals.push_back(animal);
    }

    /*
        detectAnimals(), çevrede bulunan hayvanlar listesinden,
        bu hayvanýn algýlama menzilinde olup olmadýðýný kontrol eder.
        Ardýndan, rastgelelik + uzaklýk faktörüyle tespit gerçekleþip gerçekleþmeyeceðini belirler.
    */
    void detectAnimals(const std::vector<Animal*>& animalsInRange) {
        detectedAnimals.clear();
        for (Animal* other : animalsInRange) {
            if (other != this) {
                double distance = getDistance(other->getX(), other->getY());

                double kk = 0.65;
                double probability_of_detection = (0.5 + this->detection_skill - other->getCurrentStealth()) * kk;
                probability_of_detection *= exp(-distance / this->detection_range);
                probability_of_detection = std::clamp(probability_of_detection, 0.0, 1.0);

                double randomRoll = rand() / static_cast<double>(RAND_MAX);

                if (randomRoll < probability_of_detection) {
                    detectedAnimals.push_back(other);
                }
            }
        }
    }

    void addDetectedEntity(Entity* entity) {
        detectedEntities.push_back(entity);
    }

    /*
        detectEntities(), çevredeki tüm Entity tiplerini tarar (bitkiler dahil).
        Algýlama menzili içindeki Entity'leri detectedEntities'e ekler.
    */
    void detectEntities(const std::vector<Entity*>& entitiesInRange) {
        detectedEntities.clear();
        for (const auto& entity : entitiesInRange) {
            if (isInDetectionZone(entity->getX(), entity->getY())) {
                addDetectedEntity(entity);
            }
        }
    }

    void addDetectedPlant(Plant* plant) {
        detectedPlants.push_back(plant);
    }

    /*
        detectPlants(), bitkiler özelinde algýlama fonksiyonudur.
        Algýlanan bitkiler detectedPlants vektörüne eklenir.
    */
    void detectPlants(const std::vector<Plant*>& plantsInRange) {
        detectedPlants.clear();
        for (const auto& plant : plantsInRange) {
            if (isInDetectionZone(plant->getX(), plant->getY())) {
                addDetectedPlant(plant);
            }
        }
    }

    /*
        createOffspring(), çiftleþme sonucunda yavrularýn genetik kombinasyonlarýný (ve mutasyonlarýný) hesaplar.
        Yavrular, diþi hayvanýn hamile (isPregnant) kalmasý ile doðum kuyruðuna eklenecektir.
    */
    void createOffspring(Animal* partner) {
        std::mt19937 generator(std::random_device{}());
        std::uniform_real_distribution<double> distribution(-1.0, 1.0);

        double mutation_rate = 0.175;
        auto mutate = [mutation_rate, &generator, &distribution](double value, double limitMax, double limitMin) {
            double mid = (limitMax + limitMin) / 2;
            double limiter = 1 - (abs(value - mid) / mid);
            if (limiter > 0 && limiter < 1) {
                return value + (distribution(generator) * mutation_rate * limiter);
            }
            else {
                return value;
            }
            };

        double offspring_speed = mutate((speed_coefficient + partner->speed_coefficient) / 2,
            animalLimitMax[species].speed,
            animalLimitMin[species].speed);
        double offspring_detection = (detection_range + partner->detection_range) / 2;
        double offspring_stealth = mutate((stealth_level + partner->stealth_level) / 2,
            animalLimitMax[species].stealthLevel,
            animalLimitMin[species].stealthLevel);
        double offspring_detection_skill = mutate((detection_skill + partner->detection_skill) / 2,
            animalLimitMax[species].detectionSkill,
            animalLimitMin[species].detectionSkill);

        double offspring_x = x_coordinate + (rand() % 10 - 5);
        double offspring_y = y_coordinate + (rand() % 10 - 5);

        // Diþi olan hamile kalýr ve doðum verileri Womb'a eklenir
        if (!male) {
            isPregnant = true;
            Womb = { offspring_x, offspring_y, offspring_speed, offspring_detection, offspring_stealth, offspring_detection_skill };

            reproduction_cooldown = animalTemplates[species].reproductionCooldown
                + (std::mt19937{ std::random_device{}() }() % reproductionCooldownRandom[species]);

            partner->reproduction_cooldown = animalTemplates[species].reproductionCooldown
                + (std::mt19937{ std::random_device{}() }() % reproductionCooldownRandom[species]);
        }
        else {
            partner->isPregnant = true;
            partner->Womb = { offspring_x, offspring_y, offspring_speed, offspring_detection, offspring_stealth, offspring_detection_skill };

            partner->reproduction_cooldown = animalTemplates[species].reproductionCooldown
                + (std::mt19937{ std::random_device{}() }() % reproductionCooldownRandom[species]);

            reproduction_cooldown = animalTemplates[species].reproductionCooldown
                + (std::mt19937{ std::random_device{}() }() % reproductionCooldownRandom[species]);
        }
    }

    /*
        updateDeath(), hayvanýn ölüm zamaný (death_time) geldi mi kontrol eder.
        Yaþ, death_time'ý aþarsa hayvanýn saðlýðýný 0 yapar.
    */
    void updateDeath() {
        if (age >= death_time) {
            setHealth(0);
            maxHealth = 0;
        }
        age++;
    }

    /*
        updateReproduction(), üreme bekleme süresini (cooldown) azaltýr (her adým).
    */
    void updateReproduction() {
        if (reproduction_cooldown > 0) {
            reproduction_cooldown--;
        }
    }

    /*
        updateStealthLevelBasedOnState(), hayvanýn durumuna (state) göre
        anlýk gizlilik deðerini (current_stealth) günceller.
    */
    void updateStealthLevelBasedOnState() {
        double base_stealth = stealth_level;
        double adjustment = 0.0;

        switch (state) {
        case Idle:
        case Wandering:
            adjustment = 0.1;
            break;
        case LookForFood:
        case Flee:
            adjustment = -0.1;
            break;
        case LookForPartner:
            adjustment = 0.0;
            break;
        default:
            break;
        }

        current_stealth = base_stealth + adjustment;
        current_stealth = std::clamp(current_stealth, 0.0, 0.5);
    }

    /*
        Animal için mevcut state (durum) enum deðerleri:
        Idle, Wandering, LookForFood, Flee, LookForPartner
    */
    enum State {
        Idle,
        Wandering,
        LookForFood,
        Flee,
        LookForPartner
    };

    /*
        updateState(), hayvanýn açlýðý, saðlýðý ve algýladýðý avcýlar gibi faktörlere bakarak
        hangi durumda (state) olmasý gerektiðine karar verir.
    */
    void updateState() {
        state = Wandering;
        is_ready_to_reproduce = false;

        // Avcý hayvaný tespit edildiyse, "Flee" durumu
        for (const auto& predator : detectedAnimals) {
            if (foodChainMatrix[predator->getSpecies()][species] == 1 && predator->getState() == 2) {
                state = Flee;
                return;
            }
        }

        // Üremeye hazýr olma koþullarý
        if (hunger < maxHunger * 0.5 && health > maxHealth * 0.6) {
            state = LookForPartner;
            is_ready_to_reproduce = true;
            return;
        }

        // Açlýk durumu
        if (hunger >= maxHunger * 0.6) {
            state = LookForFood;
        }
        else {
            state = Wandering;
        }
    }

    /*
        update(), her simülasyon adýmýnda hayvanýn neler yapacaðýný belirler:
         1) Yaþlanma (applyAging)
         2) State güncelleme (updateState)
         3) Reproduction cooldown azaltma (updateReproduction)
         4) Hamilelik durumu (updatePregnancy)
         5) Gizlilik güncellemesi (updateStealthLevelBasedOnState)
         6) Davranýþ (state'e göre hareket, beslenme, vb.)
         7) Ölüm kontrolü (updateDeath)
    */
    void update() {
        applyAging();
        updateState();
        updateReproduction();
        updatePregnancy();
        updateStealthLevelBasedOnState();

        // Bazý sabitler (deneysel)
        double idleHealthGain = 0.5;
        double idleHungerIncrease = 0.015;
        double fightOrFleeHungerIncrease = 0.025;
        double healthStarvationDecrease = 1;
        double idleSpeed = 1;
        double fightFlightSpeed = 2;

        double currentSpeedCoefficient = 0.7 + ((maxHunger - hunger) / maxHunger) * 0.25;
        health = clamp(health, 0.0, maxHealth + 1);
        hunger = clamp(hunger, 0.0, maxHunger + 1);

        if (hunger >= maxHunger) {
            health -= healthStarvationDecrease;
        }

        switch (state) {
        case Idle:
            // Dinlenme: açlýk yavaþ artar, saðlýk biraz düzelir
            hunger += idleHungerIncrease / 2;
            health += idleHealthGain * 2;
            break;

        case Wandering:
            moveRandomly();
            hunger += idleHungerIncrease;
            health += idleHealthGain;
            break;

        case LookForFood: {
            bool isHerbivore = foodChainMatrix[species][NUM_ANIMALS];
            double eatRange = 1.0;
            double foodHungerDecrease = 80;

            if (isHerbivore) {
                // Otçul hayvan bitki arar
                if (!detectedPlants.empty()) {
                    Plant* bestPlant = nullptr;
                    double bestBenefit = 0.0;

                    // En iyi bitkiyi bul (fayda hesaplama)
                    for (const auto& plant : detectedPlants) {
                        double distance = getDistance(plant->getX(), plant->getY());
                        double plantFood = plant->getFood();
                        double benefit = plantFood - (distance / current_speed * idleHungerIncrease);

                        if (benefit > bestBenefit) {
                            bestBenefit = benefit;
                            bestPlant = plant;
                        }
                    }

                    // Elde en iyi bitki varsa, ona git ve ye
                    if (bestPlant) {
                        if (getDistance(bestPlant->getX(), bestPlant->getY()) <= eatRange) {
                            double foodTaken = std::min(foodHungerDecrease, bestPlant->getFood());
                            bestPlant->setFood(bestPlant->getFood() - foodTaken);
                            hunger -= foodTaken / 2;
                        }
                        current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                        moveTowards(bestPlant->getX(), bestPlant->getY());
                    }
                    else {
                        // Faydalý bitki yoksa rastgele hareket
                        current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                        moveRandomly();
                    }
                }
                else {
                    current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                    moveRandomly();
                }
                hunger += idleHungerIncrease;
                health += idleHealthGain;
            }
            else {
                // Etçil hayvan av arar
                if (!detectedAnimals.empty() || !currentTarget) {
                    double bestBenefit = 0;
                    double attackRange = 3;

                    // Eðer henüz bir hedef yoksa, en iyi avý seç
                    if (currentTarget == nullptr) {
                        for (auto prey : detectedAnimals) {
                            if (foodChainMatrix[species][prey->getSpecies()] == 1) {
                                double distance = getDistance(prey->getX(), prey->getY());
                                double preyFoodCapacity = prey->getFoodCapacity();
                                double chaseCost = distance / current_speed * fightOrFleeHungerIncrease;
                                double benefit = preyFoodCapacity - chaseCost;

                                if (benefit > bestBenefit) {
                                    bestBenefit = benefit;
                                    currentTarget = prey;
                                }
                            }
                        }
                    }
                    // Hedef (currentTarget) ölmüþ veya menzil dýþýna çýkmýþsa sýfýrla
                    if (currentTarget) {
                        if (currentTarget->getHealth() <= 0 || getDistance(currentTarget->getX(), currentTarget->getY()) > detection_range) {
                            currentTarget = nullptr;
                        }
                    }
                    // Hedef hala uygun
                    if (currentTarget) {
                        double distToTarget = getDistance(currentTarget->getX(), currentTarget->getY());
                        if (distToTarget <= attackRange) {
                            // Saldýr
                            currentTarget->setHealth(currentTarget->getHealth() - 300);
                            cout << "Hayvan ID: " << currentTarget->getId() << ", tur: "
                                << animalNames[currentTarget->getSpecies()]
                                << " saldiriyi aldi. Saldiran ID: " << id
                                << ", tur: " << animalNames[species] << "\n";

                            if (currentTarget->getHealth() <= 0) {
                                hunger -= currentTarget->getFoodCapacity();
                            }
                        }
                        else if (distToTarget <= detection_range) {
                            // Hedefe doðru koþ
                            current_speed = speed_coefficient * fightFlightSpeed * currentSpeedCoefficient;
                            moveTowards(currentTarget->getX(), currentTarget->getY());
                        }
                        else {
                            currentTarget = nullptr;
                        }
                    }
                    else {
                        current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                        moveRandomly();
                    }
                }
                else {
                    current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                    moveRandomly();
                }
                hunger += idleHungerIncrease;
                health += idleHealthGain;
            }
            break;
        }
        case Flee:
            // Kaçma durumu
            if (!detectedAnimals.empty()) {
                double totalWeightedX = 0.0;
                double totalWeightedY = 0.0;
                double totalWeight = 0.0;

                for (const auto& predator : detectedAnimals) {
                    if (foodChainMatrix[predator->getSpecies()][species] == 1) {
                        double distance = getDistance(predator->getX(), predator->getY());
                        double speed = predator->getSpeed();

                        if (distance > 0) {
                            double weight = speed / distance;
                            totalWeightedX += (predator->getX() * weight);
                            totalWeightedY += (predator->getY() * weight);
                            totalWeight += weight;
                        }
                    }
                }

                if (totalWeight > 0.0) {
                    double averageX = totalWeightedX / totalWeight;
                    double averageY = totalWeightedY / totalWeight;

                    double oppositeAngle = atan2(y_coordinate - averageY, x_coordinate - averageX);
                    turn(oppositeAngle);
                    current_speed = speed_coefficient * fightFlightSpeed * currentSpeedCoefficient;
                    moveForward();
                    hunger += fightOrFleeHungerIncrease;
                }
                else {
                    current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                    moveRandomly();
                    hunger += idleHungerIncrease;
                    health += idleHealthGain;
                }
            }
            else {
                current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                moveRandomly();
                hunger += idleHungerIncrease;
                health += idleHealthGain;
            }
            break;

        case LookForPartner: {
            // Eþ aramak
            int targetAnimalID = -1;
            double minDistance = std::numeric_limits<double>::max();
            Animal* potentialPartner = nullptr;

            for (auto other : detectedAnimals) {
                if (other->species == species
                    && other->is_ready_to_reproduce
                    && reproduction_cooldown == 0
                    && other != this
                    && canMateWith(other))
                {
                    double distance = getDistance(other->x_coordinate, other->y_coordinate);
                    if (distance < minDistance) {
                        minDistance = distance;
                        potentialPartner = other;
                    }
                }
            }

            if (potentialPartner) {
                moveTowards(potentialPartner->getX(), potentialPartner->getY());
                if (minDistance <= 3.0) {
                    createOffspring(potentialPartner);
                    is_ready_to_reproduce = false;
                    potentialPartner->is_ready_to_reproduce = false;

                    cout << "Hayvan ID: " << id << " (" << animalNames[species]
                        << (male ? ", Erkek" : ", Disi") << ") , ID: "
                        << potentialPartner->getId() << " (" << animalNames[potentialPartner->getSpecies()]
                        << (potentialPartner->isMale() ? ", Erkek" : ", Disi")
                        << ") ile eslesti.\n\n";
                }
            }
            else {
                current_speed = speed_coefficient * idleSpeed * currentSpeedCoefficient;
                moveRandomly();
                hunger += idleHungerIncrease;
                health += idleHealthGain;
            }
            break;
        }
        }

        updateDeath();
    }

    /*
        removeTarget(), currentTarget hedefi ölmüþse veya baþka bir nedenle
        bu hayvandan çýkarýlmak istenirse çaðrýlýr.
    */
    void removeTarget(Animal* target) {
        if (currentTarget == target) {
            currentTarget = nullptr;
        }
    }
};

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    QuadTree veri yapýsý, 2B bir alaný (width x height) düðümlere bölerek,
    hayvan ve bitkilerin yerini hiyerarþik olarak saklamak için kullanýlýr.
    Her düðüm (bölge) 4 alt düðüme ayrýlabilir.
    Böylece belirli bir menzil içindeki varlýklarý daha hýzlý arayýp bulmak mümkün olur.
*/
class QuadTree {
private:
    static const int MAX_OBJECTS = 5;
    static const int MAX_LEVELS = 6;

    int level;
    std::vector<Animal*> animals;
    std::vector<Entity*> entities;

    QuadTree* nodes[4];
    double x, y, width, height;

    /*
        split(), Quadtree düðümünü 4 alt düðüme bölerek,
        her bir alt düðümün boyutlarýný hesaplar.
    */
    void split() {
        double subWidth = width / 2.0;
        double subHeight = height / 2.0;
        double xMid = x + subWidth;
        double yMid = y + subHeight;

        nodes[0] = new QuadTree(level + 1, x, y, subWidth, subHeight);
        nodes[1] = new QuadTree(level + 1, xMid, y, subWidth, subHeight);
        nodes[2] = new QuadTree(level + 1, x, yMid, subWidth, subHeight);
        nodes[3] = new QuadTree(level + 1, xMid, yMid, subWidth, subHeight);
    }

    /*
        getIndex(), gönderilen (objX, objY) nesnesinin hangi alt düðüme
        (0: sol üst, 1: sað üst, 2: sol alt, 3: sað alt) ait olduðunu döndürür.
    */
    int getIndex(double objX, double objY) const {
        int index = -1;
        double verticalMidpoint = x + width / 2.0;
        double horizontalMidpoint = y + height / 2.0;

        bool topQuadrant = (objY < horizontalMidpoint);
        bool bottomQuadrant = (objY >= horizontalMidpoint);

        if (objX < verticalMidpoint) {
            if (topQuadrant)        index = 0;
            else if (bottomQuadrant) index = 2;
        }
        else {
            if (topQuadrant)        index = 1;
            else if (bottomQuadrant) index = 3;
        }

        return index;
    }

    /*
        isWithinRange(), (objX, objY) merkezli bir dairenin (range yarýçaplý),
        bu QuadTree düðümü ile kesiþip kesiþmediðini kontrol eder.
    */
    bool isWithinRange(double objX, double objY, double range) const {
        double circleDistX = std::abs(objX - (x + width / 2.0));
        double circleDistY = std::abs(objY - (y + height / 2.0));

        if (circleDistX > (width / 2.0 + range))  return false;
        if (circleDistY > (height / 2.0 + range)) return false;

        if (circleDistX <= (width / 2.0)) return true;
        if (circleDistY <= (height / 2.0)) return true;

        double cornerDistSq = std::pow(circleDistX - width / 2.0, 2)
            + std::pow(circleDistY - height / 2.0, 2);
        return (cornerDistSq <= std::pow(range, 2));
    }

public:
    /*
        QuadTree kurucusu (constructor), seviye (lvl), x,y konumu, geniþlik ve yükseklik bilgisi alýr.
        nodes dizisi baþlangýçta boþ (nullptr).
    */
    QuadTree(int lvl, double xCoord, double yCoord, double w, double h)
        : level(lvl), x(xCoord), y(yCoord), width(w), height(h)
    {
        for (int i = 0; i < 4; ++i) {
            nodes[i] = nullptr;
        }
    }

    ~QuadTree() {
        clear();
    }

    // Tüm veriyi temizler, alt düðümleri siler.
    void clear() {
        animals.clear();
        for (int i = 0; i < 4; ++i) {
            if (nodes[i]) {
                nodes[i]->clear();
                delete nodes[i];
                nodes[i] = nullptr;
            }
        }
    }

    /*
        exportQuadTree(), Quadtree yapýsýný JSON formatýnda dýþarý aktarýr.
        Mevcut düðüm bilgilerini kaydeder, sonra varsa alt düðümleri için ayný iþlemi uygular (rekürsif).
    */
    void exportQuadTree(json& frame_data) const {
        json node_data;
        node_data["x"] = x;
        node_data["y"] = y;
        node_data["width"] = width;
        node_data["height"] = height;
        node_data["level"] = level;

        for (int i = 0; i < 4; ++i) {
            if (nodes[i]) {
                json child_data;
                nodes[i]->exportQuadTree(child_data);
                node_data["children"].push_back(child_data);
            }
        }
        frame_data.push_back(node_data);
    }

    /*
        insertAnimal(), hayvaný bu Quadtree düðümünün alt düðümlerine yerleþtirmeye çalýþýr.
        Eðer sýðarsa düðüme ekler, nesneler çok fazla ise split() yapar.
    */
    void insertAnimal(Animal* animal) {
        if (nodes[0]) {
            int index = getIndex(animal->getX(), animal->getY());
            if (index != -1) {
                nodes[index]->insertAnimal(animal);
                return;
            }
        }

        animals.push_back(animal);

        if (animals.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (!nodes[0]) {
                split();
            }
            auto it = animals.begin();
            while (it != animals.end()) {
                int index = getIndex((*it)->getX(), (*it)->getY());
                if (index != -1) {
                    nodes[index]->insertAnimal(*it);
                    it = animals.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }

    /*
        insertEntity(), Entity tipindeki varlýklarý (bitki vb.) quadtree'ye yerleþtirme fonksiyonu.
    */
    void insertEntity(Entity* entity) {
        if (nodes[0]) {
            int index = getIndex(entity->getX(), entity->getY());
            if (index != -1) {
                nodes[index]->insertEntity(entity);
                return;
            }
        }

        entities.push_back(entity);

        if (entities.size() > MAX_OBJECTS && level < MAX_LEVELS) {
            if (!nodes[0]) {
                split();
            }

            auto it = entities.begin();
            while (it != entities.end()) {
                int index = getIndex((*it)->getX(), (*it)->getY());
                if (index != -1) {
                    nodes[index]->insertEntity(*it);
                    it = entities.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }

    /*
        retrieveAnimal(), (objX, objY) ve range deðerine göre
        menzil içindeki hayvanlarý döndürür.
    */
    std::vector<Animal*> retrieveAnimal(Animal* self, double objX, double objY, double range) {
        std::vector<Animal*> result;

        if (nodes[0]) {
            for (int i = 0; i < 4; i++) {
                if (nodes[i]->isWithinRange(objX, objY, range)) {
                    std::vector<Animal*> retrieved = nodes[i]->retrieveAnimal(self, objX, objY, range);
                    result.insert(result.end(), retrieved.begin(), retrieved.end());
                }
            }
        }
        else {
            for (const auto& animal : animals) {
                if (animal != self && std::hypot(animal->getX() - objX, animal->getY() - objY) <= range) {
                    result.push_back(animal);
                }
            }
        }
        return result;
    }

    /*
        retrieveEntity(), ayný þekilde (objX, objY) ve range'e göre
        menzil içindeki Entity'leri döndürür (Bitkiler dahil).
    */
    std::vector<Entity*> retrieveEntity(double objX, double objY, double range) {
        std::vector<Entity*> result;

        if (nodes[0]) {
            for (int i = 0; i < 4; i++) {
                if (nodes[i]->isWithinRange(objX, objY, range)) {
                    std::vector<Entity*> retrieved = nodes[i]->retrieveEntity(objX, objY, range);
                    result.insert(result.end(), retrieved.begin(), retrieved.end());
                }
            }
        }
        else {
            for (const auto& entity : entities) {
                if (std::hypot(entity->getX() - objX, entity->getY() - objY) <= range) {
                    result.push_back(entity);
                }
            }
        }
        return result;
    }
};

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    Environment, tüm hayvanlarý, bitkileri, quadtree yapýsýný ve simülasyon döngüsünü yöneten sýnýftýr.
    - addAnimal() ve addEntity() fonksiyonlarý ile ekleme yapýlýr.
    - update() fonksiyonu, her adýmda hayvanlarýn ve bitkilerin durumunu günceller.
    - processBirthQueue() ile doðum kuyruðundaki yeni hayvanlar eklenir.
    - Veriler JSON formatýnda dosyaya kaydedilebilir.
*/
class Environment {
private:
    QuadTree* quadtree;
    int width;
    int height;

public:
    std::vector<Animal*> animals;
    std::vector<Entity*> entities;

    // Hayvanlarýn zaman içinde konum kayýtlarý
    std::map<int, std::vector<std::pair<double, double>>> animalPositions;

    int lastAnimalID = 0;
    BirthQueue birthQueue;

    Environment(int w, int h) : width(w), height(h) {
        quadtree = new QuadTree(0, 0, 0, w, h);
    }

    ~Environment() {
        for (auto& animal : animals) {
            delete animal;
        }
        for (auto& entity : entities) {
            delete entity;
        }
        delete quadtree;
    }

    /*
        addAnimal(), hayvaný ortama ekler, hayvanPositions için ID'ye uygun kayýt açar.
    */
    void addAnimal(Animal* animal) {
        animalPositions[animal->getId()] = {};
        animals.push_back(animal);
        lastAnimalID++;
    }

    /*
        removeAnimal(), ortamdan bir hayvaný güvenli þekilde çýkarýr.
    */
    void removeAnimal(Animal* animalToRemove) {
        animals.erase(std::remove(animals.begin(), animals.end(), animalToRemove), animals.end());
    }

    /*
        addEntity(), bitki vb. Entity tiplerini ortama ekler.
    */
    void addEntity(Entity* entity) {
        entities.push_back(entity);
    }

    /*
        TÜRKÇE:
        processBirthQueue(), doðum kuyruðundan (birthQueue) yeni hayvanlarý alýr ve ortama ekler.
    */
    void processBirthQueue() {
        while (birthQueue.hasPendingBirths()) {
            BirthQueue::BirthInfo birthInfo = birthQueue.dequeueBirth();
            Animal* newAnimal = new Animal(
                lastAnimalID,
                birthInfo.x,
                birthInfo.y,
                birthInfo.speed,
                birthInfo.detectionRange,
                birthInfo.species,
                birthInfo.stealthLevel,
                birthInfo.detectionSkill,
                &animals,
                &birthQueue
            );
            addAnimal(newAnimal);
            saveAnimalStaticData(basePath + "animal_static_data.json", newAnimal);
        }
    }

    /*
        update(int i), her adýmda yapýlan iþlemler:
         1) Bazý verileri kaydet (animal_dynamic_data.json).
         2) doðum kuyruðunu iþle (processBirthQueue).
         3) quadtree'yi temizle, tekrar doldur.
         4) ölmüþ hayvanlarý çýkar.
         5) hayvanlarýn update() metodunu çaðýr.
         6) quadtree'ye hayvanlarý, entity'leri yerleþtir.
         7) her hayvan için detectAnimals ve detectPlants yap.
         8) bitkilerin gýda deðerini artýr (food_rej_per_step).
         9) hayvanlarýn koordinatlarý sýnýrýn dýþýna çýkýyorsa mod alarak içeri sok.
         10) bitki verilerini kaydet (savePlantData).
    */
    void update(int i) {
        if (i % 50 == 0) {
            cout << "#################################### STEP: " << i << " ####################################\n\n";
        }

        saveAnimalDynamicData(basePath + "animal_dynamic_data.json", i);
        processBirthQueue();

        quadtree->clear();

        // Ölmüþ hayvanlarý sil
        for (auto it = animals.begin(); it != animals.end(); /* boþ */) {
            Animal* animal = *it;
            Animal* hisTarget = animal->getTarget();

            if (hisTarget != nullptr && hisTarget->getHealth() <= 0) {
                animal->removeTarget(hisTarget);
            }

            if (animal->getHealth() <= 0) {
                // Diðer hayvanlarýn detected listelerinden de çýkar
                for (auto& otherAnimal : animals) {
                    if (otherAnimal != animal) {
                        auto& detected = otherAnimal->detectedAnimals;
                        detected.erase(std::remove(detected.begin(), detected.end(), animal), detected.end());
                    }
                }
                string deadAnimalSpecies = animalNames[animal->getSpecies()];
                int deadAnimalID = animal->getId();

                it = animals.erase(it);
                delete animal;

                cout << "Hayvan ID: " << deadAnimalID << " (tur: " << deadAnimalSpecies << ") oldu.\n\n";
            }
            else {
                ++it;
            }
        }

        // Pozisyon kaydý al ve hayvanlarý güncelle
        for (auto& animal : animals) {
            double x = animal->getX();
            double y = animal->getY();
            int animalID = animal->getId();
            animalPositions[animalID].emplace_back(x, y);

            animal->update();
        }

        // Quadtree yeniden doldur
        for (auto& animal : animals) {
            quadtree->insertAnimal(animal);
        }
        for (auto& entity : entities) {
            quadtree->insertEntity(entity);
        }

        // Her hayvan, algý menzilindeki hayvan ve bitkileri belirlesin
        for (auto& animal : animals) {
            animal->detectAnimals(quadtree->retrieveAnimal(animal, animal->getX(), animal->getY(), animal->getRange()));

            std::vector<Entity*> entitiesInRange = quadtree->retrieveEntity(animal->getX(), animal->getY(), animal->getRange());
            std::vector<Plant*> plantsInRange;
            for (auto* entity : entitiesInRange) {
                Plant* plant = dynamic_cast<Plant*>(entity);
                if (plant) {
                    plantsInRange.push_back(plant);
                }
            }
            animal->detectPlants(plantsInRange);
        }

        // Bitkiler, her adým food_rej_per_step kadar kendini yeniler
        std::vector<Plant*> plants;
        for (auto* entity : entities) {
            Plant* plant = dynamic_cast<Plant*>(entity);
            if (plant) {
                plants.push_back(plant);
            }
        }
        for (auto* plant : plants) {
            if (plant->getFood() < plant->getMaxFood()) {
                plant->setFood(plant->getFood() + food_rej_per_step);
            }
        }

        // Hayvanlar ortam sýnýrýný aþarsa, mod alma ile döndür
        for (auto& animal : animals) {
            double x = animal->getX();
            double y = animal->getY();
            x = fmod(x + width, width);
            y = fmod(y + height, height);
            animal->setX(x);
            animal->setY(y);
        }

        savePlantData(basePath + "plant_data1.json", i);
        //exportData(basePath + "quadtree_data1.json", i); // Opsiyonel
    }

    /*
        exportData(), quadtree yapýsýný JSON'a kaydeder.
        Bu örnekte pasif konumdadýr (isteyen açabilir).
    */
    void exportData(const std::string& filename, int frame) const {
        static bool firstFrame = true;
        json frame_data;

        quadtree->exportQuadTree(frame_data);

        json step_entry;
        step_entry["frame"] = frame;
        step_entry["quadtree"] = frame_data;

        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
            if (firstFrame) {
                file << "";
                firstFrame = false;
            }
            else {
                file << ",";
            }
            file << step_entry.dump(4);
            file.close();
        }
    }

    /*
        saveAnimalStaticData(), yeni doðan hayvanlarýn sabit özelliklerini
        (ör. species, is_herbivore, speed vb.) JSON dosyasýna ekler.
    */
    void saveAnimalStaticData(const std::string& filename, const Animal* newAnimal) const {
        static bool isFirstStaticWrite = true;

        json animalData;
        animalData["id"] = newAnimal->getId();
        animalData["species"] = newAnimal->getSpecies();
        animalData["species_name"] = animalNames[newAnimal->getSpecies()];
        animalData["is_herbivore"] = foodChainMatrix[newAnimal->getSpecies()][NUM_ANIMALS];
        animalData["speed"] = newAnimal->getSpeed();
        animalData["stealth_level"] = newAnimal->getStealthLevel();
        animalData["detection_skill"] = newAnimal->getDetectionSkill();
        animalData["detection_range"] = newAnimal->getRange();

        std::ofstream file(filename, std::ios_base::app);
        if (file.is_open()) {
            if (isFirstStaticWrite) {
                file << "\n";
                isFirstStaticWrite = false;
            }
            else {
                file << ",\n";
            }
            file << std::setw(4) << animalData;
            file.close();
        }
        else {
            std::cerr << "Dosya acma hatasi (static data): " << filename << std::endl;
        }
    }

    /*
        saveAnimalDynamicData(), her adýmda hayvanlarýn deðiþken verilerini
        (x, y, health, hunger, state vb.) JSON dosyasýna ekler.
    */
    void saveAnimalDynamicData(const std::string& filename, int frame) const {
        static bool isFirstDynamicWrite = true;
        json frameData;
        frameData["step"] = frame;
        frameData["data"] = json::array();

        for (const auto& animal : animals) {
            json animalData;
            animalData["id"] = animal->getId();
            animalData["x"] = animal->getX();
            animalData["y"] = animal->getY();
            animalData["health"] = animal->getHealth();
            animalData["hunger"] = animal->getHunger();
            animalData["state"] = animal->getState();
            frameData["data"].push_back(animalData);
        }

        std::ofstream file(filename, std::ios_base::app);
        if (file.is_open()) {
            if (isFirstDynamicWrite) {
                file << "\n";
                isFirstDynamicWrite = false;
            }
            else {
                file << ",\n";
            }
            file << std::setw(4) << frameData;
            file.close();
        }
        else {
            std::cerr << "Dosya acma hatasi (dynamic data): " << filename << std::endl;
        }
    }

    /*
        savePlantData(), her adýmda bitkilerin (food) deðerlerini JSON'a ekler.
    */
    void savePlantData(const std::string& filename, int step) const {
        static bool isFirstPlantWrite = true;
        json stepData;
        stepData["step"] = step;
        stepData["plants"] = json::array();

        for (const auto& entity : entities) {
            const Plant* plant = dynamic_cast<const Plant*>(entity);
            if (plant) {
                json plantData;
                plantData["x"] = plant->getX();
                plantData["y"] = plant->getY();
                plantData["food"] = plant->getFood();
                stepData["plants"].push_back(plantData);
            }
        }

        std::ofstream file(filename, std::ios_base::app);
        if (file.is_open()) {
            if (isFirstPlantWrite) {
                file << "\n";
                isFirstPlantWrite = false;
            }
            else {
                file << ",\n";
            }
            file << std::setw(4) << stepData;
            file.close();
        }
        else {
            std::cerr << "Dosya acma hatasi (bitki verisi): " << filename << std::endl;
        }
    }

    /*
        finalizeExport(), JSON dizisini kapatmak gibi son iþlemleri yapar.
    */
    void finalizeExport(const std::string& filename) const {
        std::ofstream file(filename, std::ios_base::app);
        if (file.is_open()) {
            file << "\n]";
            file.close();
        }
        else {
            std::cerr << "Dosya acma hatasi (finalize): " << filename << std::endl;
        }
    }

    /*
        clearFile(), verilen dosyayý sýfýrlar (içini siler)
        ve JSON dizisine baþlamak için "[" karakterini yazar.
    */
    void clearFile(const std::string& filePath) {
        std::ofstream file(filePath, std::ofstream::out | std::ofstream::trunc);
        if (file.is_open()) {
            file << "[";
            file.close();
        }
    }
};

/*----------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*
    main() fonksiyonunda:
     - Ortam oluþturulur.
     - Hayvan türlerine göre baþlangýç populasyonu oluþturulur.
     - Bitkiler (Plant) eklenir.
     - Belirli adým sayýsý (steps) boyunca simülasyon çalýþýr.
     - Adým sonunda veriler JSON dosyalarýna yazýlýr.
     - Simülasyon bitince Python scripti çaðrýlabilir.
*/
int main() {

    //ios_base::sync_with_stdio(false);

    int width = 500;
    int height = 500;
    int steps = 55000;
    int offset = 222;

    Environment env(width, height);

    int numAnimals = 50;
    int numEntities = 50;

    // Verilerin kaydedileceði JSON dosyalarýný temizle (baþlangýç ayarlarý).
    env.clearFile(basePath + "plant_data1.json");
    env.clearFile(basePath + "quadtree_data1.json");
    env.clearFile(basePath + "animal_static_data.json");
    env.clearFile(basePath + "animal_dynamic_data.json");

    std::mt19937 mt(time(nullptr));

    // Aðýrlýk daðýlýmý (probabilityRanges) oluþturma
    std::vector<int> probablityRanges;
    probablityRanges.push_back(0);
    for (int weight : animalSpeciesWeights) {
        probablityRanges.push_back(probablityRanges.back() + weight);
    }
    int sum = probablityRanges.back();

    // Rastgele hayvan populasyonu oluþturma
    for (int i = 0; i < numAnimals; i++) {
        int temp = mt() % sum;
        int species = 0;
        for (int j = 0; j < NUM_ANIMALS; j++) {
            if (temp >= probablityRanges[j] && temp < probablityRanges[j + 1]) {
                species = j;
                break;
            }
        }

        double baseSpeed = 0.6 + (std::mt19937{ std::random_device{}() }() % 100) / 130.0;
        double baseDetectionRange = 40;
        double baseStealth = (std::mt19937{ std::random_device{}() }() % 100) / 400.0;
        double baseDetection = (std::mt19937{ std::random_device{}() }() % 100) / 400.0;

        // Tür çarpanlarýný uygula
        double speciesSpeed = baseSpeed * animalTemplates[species].speed;
        double speciesDetectionRange = baseDetectionRange * animalTemplates[species].detectionRange;
        double speciesStealth = baseStealth * animalTemplates[species].stealthLevel;
        double speciesDetection = baseDetection * animalTemplates[species].detectionSkill;

        // Yeni hayvan
        Animal* animal = new Animal(
            i,
            mt() % (width - 2 * offset) + offset,
            mt() % (height - 2 * offset) + offset,
            speciesSpeed,
            speciesDetectionRange,
            species,
            speciesStealth,
            speciesDetection,
            &env.animals,
            &env.birthQueue
        );

        env.addAnimal(animal);
        env.saveAnimalStaticData(basePath + "animal_static_data.json", animal);
    }

    // Ortama bitki eklenmesi
    for (int i = 0; i < numEntities; i++) {
        Entity* entity = new Plant(
            mt() % width,
            mt() % height,
            10,
            75 + std::mt19937{ std::random_device{}() }() % 50
        );
        env.addEntity(entity);
    }

    // Tüm simülasyonun zaman ölçümü
    auto totalStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < steps; i++) {
        auto stepStart = std::chrono::high_resolution_clock::now();
        env.update(i);
        auto stepEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> stepDuration = stepEnd - stepStart;
        //cout << "Adim " << i+1 << " suresi: " << stepDuration.count() << " saniye.\n";
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalDuration = totalEnd - totalStart;

    cout << "Toplam calisma suresi: " << totalDuration.count() << " saniye.\n";
    cout << "Adim basina sure: " << totalDuration.count() / steps << " saniye.\n";

    // JSON dosyalarý için dizileri kapat
    env.finalizeExport(basePath + "plant_data1.json");
    env.finalizeExport(basePath + "quadtree_data1.json");
    env.finalizeExport(basePath + "animal_static_data.json");
    env.finalizeExport(basePath + "animal_dynamic_data.json");

    // Ýsteðe baðlý: Python scripti çalýþtýr
    std::string pythonCommand = "py C:\\Users\\Doruk\\env\\tubitak2025\\simulation.py";
    int result = std::system(pythonCommand.c_str());

    if (result == 0) {
        std::cout << "Python betigi basariyla calisti!\n";
    }
    else {
        std::cerr << "Python betigi calistirilamadi. Hata kodu: " << result << std::endl;
    }

    return result;
}