#ifndef SWJSONSCHEMA_H
#define SWJSONSCHEMA_H

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMap>
#include <QRegularExpression>
#include <QScopedPointer>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QtMath>

/**
 * @brief Forward declaration pour SwJsonSchema (utilisé par le registry).
 */
class SwJsonSchema;
using Validator = std::function<bool(const QJsonValue &, const QJsonValue &, QString *)>;


#include <functional>
#include <QJsonValue>
#include <QString>

using Validator = std::function<bool(const QJsonValue &, const QJsonValue &, QString *)>;

class KeywordJsonValidator {
public:
    // Constructeur avec un validateur
    KeywordJsonValidator(Validator validator) : m_validator(std::move(validator)) { }

    // Constructeur par défaut
    KeywordJsonValidator() = default;

    // Constructeur de copie
    KeywordJsonValidator(const KeywordJsonValidator &other)
        : m_validator(other.m_validator),
          m_jsonSchemaValidator(other.m_jsonSchemaValidator)
    {
    }

    // Opérateur d'affectation
    KeywordJsonValidator &operator=(const KeywordJsonValidator &other)
    {
        if (this != &other) {
            m_validator = other.m_validator;
            m_jsonSchemaValidator = other.m_jsonSchemaValidator;
        }
        return *this;
    }

    // Définit les règles pour le validateur
    void setRules(const QJsonValue &rules)
    {
        m_jsonSchemaValidator = rules;
    }

    // Valide les données selon les règles et le validateur
    bool validate(const QJsonValue &data, QString *erreur) const
    {
        return m_validator(m_jsonSchemaValidator, data, erreur);
    }

    // Opérateur de comparaison pour les tests ou les recherches dans des containers
    bool operator==(const KeywordJsonValidator &other) const
    {
        return m_jsonSchemaValidator == other.m_jsonSchemaValidator;
        // Note : Les lambdas ne sont pas comparables, ici on ne compare que les règles
    }

    ~KeywordJsonValidator() = default;

private:
    Validator m_validator;            ///< Fonction de validation
    QJsonValue m_jsonSchemaValidator; ///< Règles JSON du validateur
};

/**
 * @brief Classe de registre pour les schémas JSON.
 *
 * Permet de stocker et de retrouver des SwJsonSchema par leur $id et/ou $anchor.
 * Gère également la résolution basique de $ref du style "<id>#anchor" ou "#anchor".
 */
class SwJsonSchemaRegistry {
public:
    SwJsonSchemaRegistry() = default;
    ~SwJsonSchemaRegistry() = default;

    void registerSchemaByAnchor(const QString &fullAnchor, SwJsonSchema *schema)
    {
        if (!fullAnchor.isEmpty())
            m_schemasByAnchor[fullAnchor] = schema;
    }

    void registerSchemaByRef(const QString &path, SwJsonSchema *schema)
    {
        if (!path.isEmpty())
            m_schemasByRef[path] = schema;
    }

    /**
     * @brief Tente de résoudre une référence "$ref".
     *        ex: "https://example.com/sch1#someAnchor"
     *            "#/defs/foo"
     *            "#myAnchor"
     * @return Le schéma pointé, ou nullptr si introuvable.
     */
    SwJsonSchema *resolveRef(const QString &ref, const QString &baseUri, bool &found) const
    {
        found = true;
        // Résolution simplifiée : on coupe autour du '#'
        QString localBaseURI = baseUri.split("/").last();
        QString localRef = ref;
        if (localRef.startsWith(baseUri))
            localRef.remove(0, baseUri.length()); // Supprime uniquement la première occurrence au début
        if (localRef.startsWith(localBaseURI))
            localRef.remove(0, localBaseURI.length()); // Supprime uniquement la première occurrence au début


        if (m_schemasByAnchor.contains(localRef))
            return m_schemasByAnchor.value(localRef);
        QString anchor;
        QString idPart = ref;
        if (ref.contains('#')) {
            int idx = ref.indexOf('#');
            idPart = ref.left(idx).trimmed();
            anchor = ref.mid(idx + 1).trimmed(); // sans le '#'
        }

        // 1) Si idPart est vide => on cherche "#anchor" direct
        if (idPart.isEmpty()) {
            QString fullAnchor = "#" + anchor;
            QString fullLocalAnchor = baseUri + "#" + anchor;
            if (m_schemasByAnchor.contains(fullAnchor))
                return m_schemasByAnchor.value(fullAnchor);

            if (m_schemasByAnchor.contains(fullLocalAnchor))
                return m_schemasByAnchor.value(fullLocalAnchor);
        }

        // 4) Sinon, on teste "idPart#anchor"
        QString fullAnchor = baseUri + "#" + anchor;
        if (m_schemasByAnchor.contains(fullAnchor))
            return m_schemasByAnchor.value(fullAnchor);

        foreach (auto key, m_schemasByRef.keys()) {
            if (idPart != "" && key.endsWith(idPart)) {
                found = false;
                return m_schemasByRef.value(key);
            }
        }

        return nullptr;
    }

private:
    QMap<QString, SwJsonSchema *> m_schemasByAnchor; ///< Map "id#anchor" ou "#anchor" -> schéma
    QMap<QString, SwJsonSchema *> m_schemasByRef;    ///< Map "path" -> schéma
};

/**
 * @brief Classe SwJsonSchema : représente un schéma JSON, capable de valider un QJsonValue.
 *
 * - Un **seul** constructeur public : prend un chemin (ou URL) vers un fichier JSON Schema.
 * - Lit le fichier local, parse le JSON, et charge récursivement les sous-schemas.
 * - Gère $id, $anchor, $ref (résolution via un registry, si fourni).
 * - Gère if/then/else, allOf/anyOf/oneOf, etc.
 * - Évite les boucles de référence via un set de "visited".
 */
class SwJsonSchema {
public:
    enum class SchemaType
    {
        Invalid,
        String,
        Number,
        Integer,
        Boolean,
        Object,
        Array,
        Null
    };

    /**
     * @brief Constructeur unique : charge le schéma depuis un chemin (ou URL) `schemaPath`.
     * @param schemaPath  Chemin local ou URL
     * @param registry    Pointeur vers un registre de schémas (optionnel)
     */
    explicit SwJsonSchema(const QString &schemaPath, SwJsonSchema *parent = nullptr)
        : m_parent(parent),
          m_baseUri(schemaPath)
    {
        if (parent)
            m_baseUri = resolveUri(parent, schemaPath);
        // Tente d’ouvrir le fichier local
        // (si vous gérez des URLs http(s), adapter ici)
        QUrl url(schemaPath);
        QString localFile = url.isLocalFile() ? url.toLocalFile() : schemaPath;

        QFile f(localFile);
        if (!f.open(QIODevice::ReadOnly)) {
            // Erreur : on pourrait stocker un message d’erreur ou laisser un schéma "vide".
            return;
        }

        QByteArray data = f.readAll();
        f.close();

        QJsonParseError jerr;
        QJsonDocument doc = QJsonDocument::fromJson(data, &jerr);
        if (jerr.error != QJsonParseError::NoError) {
            // Erreur de parsing JSON
            return;
        }
        if (!doc.isObject()) {
            // Pas un objet JSON => schéma invalide
            return;
        }
        QJsonObject rootObj = doc.object();
        m_isValide = !rootObj.isEmpty();
        if (m_isValide)
            loadSchema(rootObj, parent);
    }

    /**
     * @brief Constructeur unique : charge le schéma depuis un chemin (ou URL) `schemaPath`.
     * @param schemaPath  Chemin local ou URL
     * @param registry    Pointeur vers un registre de schémas (optionnel)
     */
    explicit SwJsonSchema(const QJsonObject &data, SwJsonSchema *parent = nullptr) : m_parent(parent)
    {
        m_isValide = !data.isEmpty();
        if (m_isValide)
            loadSchema(data, parent);
    }

    // Copie
    SwJsonSchema(const SwJsonSchema &other)
    {
        copyFrom(other);
    }

    SwJsonSchema &operator=(const SwJsonSchema &other)
    {
        if (this == &other)
            return *this;
        copyFrom(other);
        return *this;
    }

    /**
     * @brief Destructeur
     */
    ~SwJsonSchema()
    {
        // Nettoyage
        qDeleteAll(m_prefixItemsSchemas);
        m_prefixItemsSchemas.clear();
    }

    /**
     * @brief Valide une QJsonValue contre ce schéma
     * @param value         Valeur à valider
     * @param errorMessage  Optionnel, reçoit le motif d’erreur
     * @return true si la valeur est valide, false sinon
     */
    bool validate(const QJsonValue &value, QString *errorMessage = nullptr) const
    {
        QSet<const SwJsonSchema *> visited;
        return validateInternal(value, visited, errorMessage);
    }

    bool isValide()
    {
        return m_isValide;
    }

    /**
     * @brief Enregistre une lambda pour un mot-clé personnalisé
     * @param keyWord Mot-clé
     * @param validator Lambda prenant un QJsonValue et QString& pour l'erreur
     */
    static void registerCustomKeyword(const QString &keyWord, Validator validator)
    {
        getCustomKeywordRegistry()[keyWord] = validator;
    }

private:
    // -----------------------------------------------------------------------
    //                   Méthodes de chargement
    // -----------------------------------------------------------------------
    void loadSchema(const QJsonObject &schemaObject, const SwJsonSchema *parent)
    {
        // 1) Lire $schema (optionnel)
        if (schemaObject.contains("$schema") && schemaObject.value("$schema").isString())
            m_dollarSchema = schemaObject.value("$schema").toString();

        // 2) Lire $id
        if (schemaObject.contains("$id") && schemaObject.value("$id").isString())
            m_baseUri = schemaObject.value("$id").toString().trimmed();

        // 3) Calcul du m_baseUri
        m_baseUri = resolveUri(parent, m_baseUri);

        // 5) Lire $anchor
        //        getRegistry(m_baseUri)->registerSchemaByAnchor("#", this);
        if (schemaObject.contains("$anchor") && schemaObject.value("$anchor").isString()) {
            m_dollarAnchor = schemaObject.value("$anchor").toString().trimmed();
            if (!m_dollarAnchor.isEmpty()) {
                QString fullAnchor = m_baseUri + "#" + m_dollarAnchor;
                getRegistry(m_baseUri)->registerSchemaByAnchor(fullAnchor, this);
            }
        }

        // 6) Lire $ref
        if (schemaObject.contains("$ref") && schemaObject.value("$ref").isString()) {
            m_dollarRef = schemaObject.value("$ref").toString().trimmed();
            if (!m_dollarRef.contains("$def")) {
                if (!m_dollarRef.trimmed().startsWith("#")) {
                    QStringList tmpLst = m_baseUri.split("/");
                    tmpLst.removeLast();
                    tmpLst.append(m_dollarRef.split("#").first());
                    SwJsonSchema *ref = new SwJsonSchema(tmpLst.join("/"), this);
                    if (ref->m_isValide) {
                        getRegistry(m_baseUri)->registerSchemaByRef(tmpLst.join("/"), ref);
                    }
                    else {
                        delete ref;
                        m_dollarRef = "";
                    }
                }
                else if (m_dollarRef.trimmed() == "#") {
                    // recursive reference
                    m_recursiveSchema = findMainSchema();
                }
            }
        }

        // 7.1) Charger $defs
        if (schemaObject.contains("$defs") && schemaObject.value("$defs").isObject()) {
            QJsonObject defsObj = schemaObject.value("$defs").toObject();
            for (auto it = defsObj.begin(); it != defsObj.end(); ++it) {
                if (it.value().isObject()) {
                    SwJsonSchema *def = new SwJsonSchema(it.value().toObject(), this);
                    getRegistry(m_baseUri)->registerSchemaByAnchor("#/$defs/" + it.key(), def);
                }
            }
        }
        // 7.2) Charger $defs
        if (schemaObject.contains("definitions") && schemaObject.value("definitions").isObject()) {
            QJsonObject defsObj = schemaObject.value("definitions").toObject();
            for (auto it = defsObj.begin(); it != defsObj.end(); ++it) {
                if (it.value().isObject()) {
                    SwJsonSchema *def = new SwJsonSchema(it.value().toObject(), this);
                    getRegistry(m_baseUri)->registerSchemaByAnchor("#/definitions/" + it.key(), def);
                }
            }
        }

        // 8) Lire "type"
        if (schemaObject.contains("type"))
            m_type = parseType(schemaObject.value("type"));

        // 9) enum / const
        if (schemaObject.contains("enum") && schemaObject.value("enum").isArray()) {
            QJsonArray arr = schemaObject.value("enum").toArray();
            for (const auto &v : arr)
                m_enumValues.append(v);
        }
        if (schemaObject.contains("const"))
            m_constValue = schemaObject.value("const");

        // 10) multipleOf, minimum, maximum, ...
        if (schemaObject.contains("multipleOf")) {
            m_multipleOf = schemaObject.value("multipleOf").toDouble(0.0);
            m_hasMultipleOf = true;
        }
        if (schemaObject.contains("minimum")) {
            m_minimum = schemaObject.value("minimum").toDouble(0.0);
            m_hasMinimum = true;
        }
        if (schemaObject.contains("maximum")) {
            m_maximum = schemaObject.value("maximum").toDouble(0.0);
            m_hasMaximum = true;
        }
        if (schemaObject.contains("exclusiveMinimum")) {
            const QJsonValue exMin = schemaObject.value("exclusiveMinimum");
            if (exMin.isBool()) {
                m_exclusiveMinimum = exMin.toBool(false);
            }
            else {
                m_minimum = exMin.toDouble(0.0);
                m_hasMinimum = true;
                m_exclusiveMinimum = true;
            }
        }
        if (schemaObject.contains("exclusiveMaximum")) {
            const QJsonValue exMax = schemaObject.value("exclusiveMaximum");
            if (exMax.isBool()) {
                m_exclusiveMaximum = exMax.toBool(false);
            }
            else {
                m_maximum = exMax.toDouble(0.0);
                m_hasMaximum = true;
                m_exclusiveMaximum = true;
            }
        }

        // 11) minLength / maxLength / pattern / format
        if (schemaObject.contains("minLength"))
            m_minLength = schemaObject.value("minLength").toInt(-1);
        if (schemaObject.contains("maxLength"))
            m_maxLength = schemaObject.value("maxLength").toInt(-1);
        if (schemaObject.contains("pattern")) {
            m_pattern = schemaObject.value("pattern").toString();
            m_hasPattern = true;
        }
        if (schemaObject.contains("format"))
            m_format = schemaObject.value("format").toString();

        // 12) items / prefixItems / additionalItems
        if (schemaObject.contains("items")) {
            QJsonValue val = schemaObject.value("items");
            if (val.isObject()) {
                m_itemsSchema.reset(new SwJsonSchema(val.toObject(), this));
            }
            else if (val.isArray()) {
                QJsonArray arr = val.toArray();
                for (const auto &item : arr) {
                    if (item.isObject()) {
                        SwJsonSchema *sub = new SwJsonSchema(item.toObject(), this);
                        m_prefixItemsSchemas.append(sub);
                    }
                }
            }
        }
        if (schemaObject.contains("additionalItems") && schemaObject.value("additionalItems").isObject())
            m_additionalItemsSchema.reset(new SwJsonSchema(schemaObject.value("additionalItems").toObject(), this));
        if (schemaObject.contains("minItems"))
            m_minItems = schemaObject.value("minItems").toInt(-1);
        if (schemaObject.contains("maxItems"))
            m_maxItems = schemaObject.value("maxItems").toInt(-1);
        if (schemaObject.contains("uniqueItems"))
            m_uniqueItems = schemaObject.value("uniqueItems").toBool(false);

        // 13) contains / minContains / maxContains
        if (schemaObject.contains("contains") && schemaObject.value("contains").isObject())
            m_containsSchema.reset(new SwJsonSchema(schemaObject.value("contains").toObject(), this));
        if (schemaObject.contains("minContains"))
            m_minContains = schemaObject.value("minContains").toInt(-1);
        if (schemaObject.contains("maxContains"))
            m_maxContains = schemaObject.value("maxContains").toInt(-1);

        // 14) properties / patternProperties / additionalProperties
        if (schemaObject.contains("properties") && schemaObject.value("properties").isObject()) {
            QJsonObject props = schemaObject.value("properties").toObject();
            for (auto it = props.begin(); it != props.end(); ++it) {
                if (it.value().isObject()) {
                    SwJsonSchema sub(it.value().toObject(), this);
                    m_properties.insert(it.key(), sub);
                }
            }
        }
        if (schemaObject.contains("patternProperties") && schemaObject.value("patternProperties").isObject()) {
            QJsonObject pprops = schemaObject.value("patternProperties").toObject();
            for (auto it = pprops.begin(); it != pprops.end(); ++it) {
                if (it.value().isObject()) {
                    SwJsonSchema sub(it.value().toObject(), this);
                    m_patternProperties.insert(it.key(), sub);
                }
            }
        }
        if (schemaObject.contains("additionalProperties")) {
            QJsonValue apVal = schemaObject.value("additionalProperties");
            if (apVal.isBool()) {
                if (!apVal.toBool())
                    m_additionalPropertiesIsFalse = true;
            }
            else if (apVal.isObject()) {
                m_additionalPropertiesSchema.reset(new SwJsonSchema(apVal.toObject(), this));
            }
        }

        // 15) required / dependentRequired
        if (schemaObject.contains("required") && schemaObject.value("required").isArray()) {
            QJsonArray reqArr = schemaObject.value("required").toArray();
            for (const auto &r : reqArr)
                if (r.isString())
                    m_required.insert(r.toString());
        }
        if (schemaObject.contains("dependentRequired") && schemaObject.value("dependentRequired").isObject()) {
            QJsonObject drObj = schemaObject.value("dependentRequired").toObject();
            for (auto it = drObj.begin(); it != drObj.end(); ++it) {
                if (it.value().isArray()) {
                    QStringList deps;
                    for (const auto &d : it.value().toArray())
                        deps << d.toString();
                    m_dependentRequired.insert(it.key(), deps);
                }
            }
        }

        // 16) allOf / anyOf / oneOf / not
        if (schemaObject.contains("allOf") && schemaObject.value("allOf").isArray()) {
            QJsonArray arr = schemaObject.value("allOf").toArray();
            for (const auto &sch : arr) {
                if (sch.isObject()) {
                    SwJsonSchema sub(sch.toObject(), this);
                    m_allOf.append(sub);
                }
            }
        }
        if (schemaObject.contains("anyOf") && schemaObject.value("anyOf").isArray()) {
            QJsonArray arr = schemaObject.value("anyOf").toArray();
            for (const auto &sch : arr) {
                if (sch.isObject()) {
                    SwJsonSchema sub(sch.toObject(), this);
                    m_anyOf.append(sub);
                }
            }
        }
        if (schemaObject.contains("oneOf") && schemaObject.value("oneOf").isArray()) {
            QJsonArray arr = schemaObject.value("oneOf").toArray();
            for (const auto &sch : arr) {
                if (sch.isObject()) {
                    SwJsonSchema sub(sch.toObject(), this);
                    m_oneOf.append(sub);
                }
            }
        }
        if (schemaObject.contains("not") && schemaObject.value("not").isObject())
            m_notSchema.reset(new SwJsonSchema(schemaObject.value("not").toObject(), this));
        // 17) if / then / else
        if (schemaObject.contains("if") && schemaObject.value("if").isObject())
            m_ifSchema.reset(new SwJsonSchema(schemaObject.value("if").toObject(), this));
        if (schemaObject.contains("then") && schemaObject.value("then").isObject())
            m_thenSchema.reset(new SwJsonSchema(schemaObject.value("then").toObject(), this));
        if (schemaObject.contains("else") && schemaObject.value("else").isObject())
            m_elseSchema.reset(new SwJsonSchema(schemaObject.value("else").toObject(), this));

        // 18) Si type pas défini => tenter deduceTypeFromConstraints()
        if (m_type == SchemaType::Invalid)
            deduceTypeFromConstraints();

        const auto &customKeywords = getCustomKeywordRegistry();
        foreach (const QString &key, schemaObject.keys()) {
            if (customKeywords.contains(key)) {
                KeywordJsonValidator userKey(customKeywords.value(key));
                userKey.setRules(schemaObject.value(key));
                m_internalCustomKeywordValidator.append(userKey);
            }
        }
    }

    // -----------------------------------------------------------------------
    //                   Validation (interne)
    // -----------------------------------------------------------------------
    bool validateInternal(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        if (visited.contains(this))
            return setError(errorMessage, "Récursion de schémas détectée.");
        visited.insert(this);

        // Gérer $ref
        if (!m_dollarRef.isEmpty() && m_dollarRef != "#") {
            bool isFound = false;
            SwJsonSchema *refSchema = const_cast<SwJsonSchema *>(this);
            while (!isFound && refSchema != nullptr)
                refSchema = getRegistry(refSchema->m_baseUri)->resolveRef(m_dollarRef, refSchema->m_baseUri, isFound);
            if (!refSchema)
                return setError(errorMessage, QString("Impossible de résoudre la référence '%1'.").arg(m_dollarRef));
            return refSchema->validateInternal(value, visited, errorMessage);
        }

        // if/then/else
        if (!applyConditional(value, visited, errorMessage))
            return false;

        // not
        if (m_notSchema) {
            if (m_notSchema->validateInternal(value, visited, nullptr))
                return setError(errorMessage, "Le schéma 'not' est satisfait, ce qui est interdit.");
        }

        // allOf / anyOf / oneOf
        if (!checkAllOf(value, visited, errorMessage))
            return false;
        if (!checkAnyOf(value, visited, errorMessage))
            return false;
        if (!checkOneOf(value, visited, errorMessage))
            return false;

        // enum / const
        if (!m_enumValues.isEmpty()) {
            bool found = false;
            for (auto &ev : m_enumValues) {
                if (ev == value) {
                    found = true;
                    break;
                }
            }
            if (!found)
                return setError(errorMessage, "Valeur non listée dans 'enum'.");
        }
        if (!m_constValue.isUndefined()) {
            if (m_constValue != value)
                return setError(errorMessage, "Valeur différente de 'const'.");
        }

        // Vérifier le type
        if (m_type != SchemaType::Invalid) {
            if (!checkType(value)) {
                return setError(errorMessage,
                                QString("Type invalide. Attendu: %1, reçu: %2")
                                    .arg(toString(m_type))
                                    .arg(toString(value)));
            }
        }

        // Validation détaillée
        SchemaType currentType = m_type;
        if (currentType == SchemaType::Invalid)
            currentType = getType(value);

        switch (currentType) {
            case SchemaType::String:
                if (!validateString(value, errorMessage))
                    return false;
                break;
            case SchemaType::Number:
                if (!validateNumber(value, errorMessage))
                    return false;
                break;
            case SchemaType::Integer:
                if (!validateNumber(value, errorMessage))
                    return false;
                break;
            case SchemaType::Boolean:
                // rien de spécial
                break;
            case SchemaType::Object:
                if (!validateObject(value, visited, errorMessage))
                    return false;
                break;
            case SchemaType::Array:
                if (!validateArray(value, visited, errorMessage))
                    return false;
                break;
            case SchemaType::Null:
                // rien
                break;
            case SchemaType::Invalid:
            default:
                // pas de type imposé
                break;
        }

        foreach (KeywordJsonValidator customValidator, m_internalCustomKeywordValidator) {
            if (!customValidator.validate(value, errorMessage))
                return setError(errorMessage, QString("Validation failed with error: %1").arg(*errorMessage));
        }

        return true;
    }

    // -- if/then/else, allOf, anyOf, oneOf --
    bool applyConditional(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        m_ifThenElsePropertyValidated.clear();
        if (!m_ifSchema)
            return true;
        // si ifSchema satisfait
        if (m_ifSchema->validateInternal(value, visited, nullptr)) {
            // then
            if (m_thenSchema && !m_thenSchema->validateInternal(value, visited, errorMessage))
                return false;
            if (m_thenSchema)
                m_ifThenElsePropertyValidated.unite(m_thenSchema->m_required);
        }
        else {
            // else
            if (m_elseSchema && !m_elseSchema->validateInternal(value, visited, errorMessage))
                return false;
            if (m_elseSchema)
                m_ifThenElsePropertyValidated.unite(m_elseSchema->m_required);
        }
        return true;
    }

    bool checkAllOf(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        for (int i = 0; i < m_allOf.size(); ++i) {
            if (!m_allOf[i].validateInternal(value, visited, errorMessage)) {
                return setError(errorMessage,
                                QString("Echec de allOf[%1]. %2").arg(i).arg(errorMessage ? *errorMessage : ""));
            }
        }
        return true;
    }

    bool checkAnyOf(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        if (m_anyOf.isEmpty())
            return true;
        for (int i = 0; i < m_anyOf.size(); ++i) {
            QString localErr;
            QSet<const SwJsonSchema *> visitedCopy(visited);
            if (m_anyOf[i].validateInternal(value, visitedCopy, &localErr))
                return true; // au moins un match => OK
        }
        return setError(errorMessage, "Aucun schéma dans 'anyOf' n'est satisfait.");
    }

    bool checkOneOf(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        if (m_oneOf.isEmpty())
            return true;
        int countValid = 0;
        QString lastError;
        for (int i = 0; i < m_oneOf.size(); ++i) {
            QString localErr;
            QSet<const SwJsonSchema *> visitedCopy(visited);
            if (m_oneOf[i].validateInternal(value, visitedCopy, &localErr)) {
                countValid++;
                if (countValid > 1)
                    return setError(errorMessage, "Plus d'un schéma dans 'oneOf' est satisfait.");
            }
            else {
                lastError = localErr;
            }
        }
        if (countValid == 1) {
            return true;
        }
        else {
            return setError(errorMessage,
                            QString("Aucun schéma dans 'oneOf' n'est satisfait. Dernière erreur: %1").arg(lastError));
        }
    }

    // -- validations de type --
    bool validateString(const QJsonValue &value, QString *errorMessage) const
    {
        QString str = value.toString();
        if (m_minLength >= 0 && str.size() < m_minLength)
            return setError(errorMessage, QString("Longueur trop petite: %1 < %2").arg(str.size()).arg(m_minLength));
        if (m_maxLength >= 0 && str.size() > m_maxLength)
            return setError(errorMessage, QString("Longueur trop grande: %1 > %2").arg(str.size()).arg(m_maxLength));
        if (m_hasPattern) {
            QRegularExpression re(m_pattern);
            if (!re.match(str).hasMatch())
                return setError(errorMessage, QString("Ne correspond pas au pattern: %1").arg(m_pattern));
        }
        if (!m_format.isEmpty()) {
            if (!checkFormat(str, m_format, errorMessage))
                return false;
        }
        return true;
    }

    bool validateNumber(const QJsonValue &value, QString *errorMessage) const
    {
        if (!checkType(value))
            return setError(errorMessage, QString("Value %1 is not a type double").arg(value.toString()));
        double d = value.toDouble();
        if (m_hasMultipleOf && !qFuzzyIsNull(m_multipleOf)) {
            double ratio = d / m_multipleOf;
            double frac = ratio - qFloor(ratio);
            double eps = 1e-12;
            if (qAbs(frac) > eps && qAbs(frac - 1.0) > eps)
                return setError(errorMessage, QString("%1 n'est pas multiple de %2").arg(d).arg(m_multipleOf));
        }
        if (m_hasMinimum) {
            if (m_exclusiveMinimum) {
                if (!(d > m_minimum))
                    return setError(errorMessage, QString("Doit être > %1 (exclusiveMinimum)").arg(m_minimum));
            }
            else {
                if (d < m_minimum)
                    return setError(errorMessage, QString("Doit être >= %1").arg(m_minimum));
            }
        }
        if (m_hasMaximum) {
            if (m_exclusiveMaximum) {
                if (!(d < m_maximum))
                    return setError(errorMessage, QString("Doit être < %1 (exclusiveMaximum)").arg(m_maximum));
            }
            else {
                if (d > m_maximum)
                    return setError(errorMessage, QString("Doit être <= %1").arg(m_maximum));
            }
        }
        return true;
    }

    bool validateObject(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        if (!value.isObject())
            return setError(errorMessage, "La valeur n'est pas un objet.");
        QJsonObject obj = value.toObject();

        // required
        for (auto &req : m_required)
            if (!obj.contains(req))
                return setError(errorMessage, QString("La propriété requise '%1' est manquante.").arg(req));

        // dependentRequired
        for (auto it = m_dependentRequired.begin(); it != m_dependentRequired.end(); ++it) {
            if (obj.contains(it.key())) {
                for (auto &dep : it.value()) {
                    if (!obj.contains(dep)) {
                        return setError(errorMessage,
                                        QString("La propriété '%1' est requise car '%2' est présent.")
                                            .arg(dep)
                                            .arg(it.key()));
                    }
                }
            }
        }

        // properties
        for (auto it = m_properties.begin(); it != m_properties.end(); ++it) {
            if (obj.contains(it.key())) {
                QString localErr;
                QSet<const SwJsonSchema *> visitedCopy(visited);
                if (!it.value().validateInternal(obj.value(it.key()), visitedCopy, &localErr))
                    return setError(errorMessage, QString("Propriété '%1' invalide: %2").arg(it.key()).arg(localErr));
            }
        }

        // patternProperties
        for (auto pit = m_patternProperties.begin(); pit != m_patternProperties.end(); ++pit) {
            QRegularExpression re(pit.key());
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (re.match(it.key()).hasMatch()) {
                    QString localErr;
                    QSet<const SwJsonSchema *> visitedCopy(visited);
                    if (!pit.value().validateInternal(it.value(), visitedCopy, &localErr)) {
                        return setError(errorMessage,
                                        QString("Propriété '%1' invalide (patternProperties / %2): %3")
                                            .arg(it.key())
                                            .arg(pit.key())
                                            .arg(localErr));
                    }
                }
            }
        }

        // additionalProperties
        if (m_additionalPropertiesIsFalse) {
            // on refuse toute propriété non listée
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!m_properties.contains(it.key()) && !matchesAnyPattern(it.key()) &&
                    !m_ifThenElsePropertyValidated.contains(it.key()))
                {
                    return setError(errorMessage,
                                    QString("Propriété '%1' non autorisée (additionalProperties=false).").arg(it.key()));
                }
            }
        }
        else if (m_additionalPropertiesSchema) {
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!m_properties.contains(it.key()) && !matchesAnyPattern(it.key()) &&
                    !m_ifThenElsePropertyValidated.contains(it.key()))
                {
                    QString localErr;
                    QSet<const SwJsonSchema *> visitedCopy(visited);
                    if (!m_additionalPropertiesSchema->validateInternal(it.value(), visitedCopy, &localErr)) {
                        return setError(errorMessage,
                                        QString("Propriété '%1' invalide (additionalProperties): %2")
                                            .arg(it.key())
                                            .arg(localErr));
                    }
                }
            }
        }

        if (m_recursiveSchema) {
            QScopedPointer<SwJsonSchema> _circularRef(new SwJsonSchema(*m_recursiveSchema));
            // properties
            for (auto it = _circularRef->m_properties.begin(); it != _circularRef->m_properties.end(); ++it) {
                if (obj.contains(it.key())) {
                    QString localErr;
                    QSet<const SwJsonSchema *> visitedCopy(visited);
                    if (!it.value().validateInternal(obj.value(it.key()), visitedCopy, &localErr)) {
                        return setError(errorMessage,
                                        QString("Propriété '%1' invalide: %2").arg(it.key()).arg(localErr));
                    }
                }
            }


            if (_circularRef->m_additionalPropertiesIsFalse) {
                // on refuse toute propriété non listée
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    if (!_circularRef->m_properties.contains(it.key()) && !_circularRef->matchesAnyPattern(it.key()) &&
                        !_circularRef->m_ifThenElsePropertyValidated.contains(it.key()))
                    {
                        return setError(errorMessage,
                                        QString("Propriété '%1' non autorisée (additionalProperties=false).")
                                            .arg(it.key()));
                    }
                }
            }
            else if (_circularRef->m_additionalPropertiesSchema) {
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    if (!_circularRef->m_properties.contains(it.key()) && !_circularRef->matchesAnyPattern(it.key()) &&
                        !_circularRef->m_ifThenElsePropertyValidated.contains(it.key()))
                    {
                        QString localErr;
                        QSet<const SwJsonSchema *> visitedCopy(visited);
                        if (!_circularRef->m_additionalPropertiesSchema->validateInternal(it.value(), visitedCopy,
                                                                                          &localErr))
                        {
                            return setError(errorMessage,
                                            QString("Propriété '%1' invalide (additionalProperties): %2")
                                                .arg(it.key())
                                                .arg(localErr));
                        }
                    }
                }
            }
        }
        return true;
    }

    bool validateArray(const QJsonValue &value, QSet<const SwJsonSchema *> &visited, QString *errorMessage) const
    {
        if (!value.isArray())
            return setError(errorMessage, "La valeur n'est pas un tableau (array).");
        QJsonArray arr = value.toArray();

        if (m_minItems >= 0 && arr.size() < m_minItems)
            return setError(errorMessage, QString("Trop peu d'éléments: %1 < %2").arg(arr.size()).arg(m_minItems));
        if (m_maxItems >= 0 && arr.size() > m_maxItems)
            return setError(errorMessage, QString("Trop d'éléments: %1 > %2").arg(arr.size()).arg(m_maxItems));
        if (m_uniqueItems) {
            for (int i = 0; i < arr.size(); ++i) {
                for (int j = i + 1; j < arr.size(); ++j)
                    if (arr[i] == arr[j])
                        return setError(errorMessage, "Doublon trouvé alors que uniqueItems=true.");
            }
        }

        // items / prefixItems
        if (m_itemsSchema) {
            for (int i = 0; i < arr.size(); ++i) {
                QString localErr;
                QSet<const SwJsonSchema *> visitedCopy(visited);
                if (!m_itemsSchema->validateInternal(arr[i], visitedCopy, &localErr))
                    return setError(errorMessage, QString("Element [%1] invalide: %2").arg(i).arg(localErr));
            }
        }
        else if (!m_prefixItemsSchemas.isEmpty()) {
            int i = 0;
            for (; i < arr.size() && i < m_prefixItemsSchemas.size(); ++i) {
                QString localErr;
                QSet<const SwJsonSchema *> visitedCopy(visited);
                if (!m_prefixItemsSchemas[i]->validateInternal(arr[i], visitedCopy, &localErr)) {
                    return setError(errorMessage,
                                    QString("Element [%1] invalide (prefixItems): %2").arg(i).arg(localErr));
                }
            }
            // Au-delà de prefixItems, on applique additionalItemsSchema si défini
            if (i < arr.size()) {
                if (m_additionalItemsSchema) {
                    for (; i < arr.size(); ++i) {
                        QString localErr;
                        QSet<const SwJsonSchema *> visitedCopy(visited);
                        if (!m_additionalItemsSchema->validateInternal(arr[i], visitedCopy, &localErr)) {
                            return setError(errorMessage,
                                            QString("Element [%1] invalide (additionalItems): %2").arg(i).arg(localErr));
                        }
                    }
                }
                // Sinon, on accepte (draft 2019-09).
            }
        }

        // contains
        if (m_containsSchema) {
            int count = 0;
            for (int i = 0; i < arr.size(); ++i) {
                QSet<const SwJsonSchema *> visitedCopy(visited);
                if (m_containsSchema->validateInternal(arr[i], visitedCopy, nullptr))
                    count++;
            }
            if (m_minContains >= 0 && count < m_minContains) {
                return setError(errorMessage,
                                QString("Pas assez d'éléments correspondant à 'contains': %1 < %2.")
                                    .arg(count)
                                    .arg(m_minContains));
            }
            if (m_maxContains >= 0 && count > m_maxContains) {
                return setError(errorMessage,
                                QString("Trop d'éléments correspondant à 'contains': %1 > %2.")
                                    .arg(count)
                                    .arg(m_maxContains));
            }
            if (m_minContains < 0 && m_maxContains < 0 && count == 0)
                return setError(errorMessage, "Aucun élément ne satisfait 'contains'.");
        }

        return true;
    }

    // -----------------------------------------------------------------------
    //                Outils
    // -----------------------------------------------------------------------
    bool matchesAnyPattern(const QString &propertyName) const
    {
        for (auto it = m_patternProperties.cbegin(); it != m_patternProperties.cend(); ++it) {
            QRegularExpression re(it.key());
            if (re.match(propertyName).hasMatch())
                return true;
        }
        return false;
    }

    // -----------------------------------------------------------------
    //             Outils annexes
    // -----------------------------------------------------------------
    void copyFrom(const SwJsonSchema &other)
    {
        // On ne copie pas le registry => dépend du design
        // (on pourrait le recopier, ou le passer en paramètre)
        m_dollarSchema = other.m_dollarSchema;
        m_baseUri = other.m_baseUri;
        m_dollarAnchor = other.m_dollarAnchor;
        m_dollarRef = other.m_dollarRef;

        m_type = other.m_type;
        m_enumValues = other.m_enumValues;
        m_constValue = other.m_constValue;

        m_multipleOf = other.m_multipleOf;
        m_hasMultipleOf = other.m_hasMultipleOf;
        m_minimum = other.m_minimum;
        m_maximum = other.m_maximum;
        m_exclusiveMinimum = other.m_exclusiveMinimum;
        m_exclusiveMaximum = other.m_exclusiveMaximum;
        m_hasMinimum = other.m_hasMinimum;
        m_hasMaximum = other.m_hasMaximum;

        m_minLength = other.m_minLength;
        m_maxLength = other.m_maxLength;
        m_pattern = other.m_pattern;
        m_hasPattern = other.m_hasPattern;
        m_format = other.m_format;

        m_minItems = other.m_minItems;
        m_maxItems = other.m_maxItems;
        m_uniqueItems = other.m_uniqueItems;

        // copier m_itemsSchema, m_additionalItemsSchema
        m_itemsSchema.reset(other.m_itemsSchema ? new SwJsonSchema(*other.m_itemsSchema) : nullptr);
        m_additionalItemsSchema.reset(other.m_additionalItemsSchema ? new SwJsonSchema(*other.m_additionalItemsSchema)
                                                                    : nullptr);

        // copier prefixItems
        for (auto *p : m_prefixItemsSchemas)
            delete p;
        m_prefixItemsSchemas.clear();
        for (auto *p : other.m_prefixItemsSchemas)
            m_prefixItemsSchemas.append(new SwJsonSchema(*p));

        // contains
        m_containsSchema.reset(other.m_containsSchema ? new SwJsonSchema(*other.m_containsSchema) : nullptr);
        m_minContains = other.m_minContains;
        m_maxContains = other.m_maxContains;

        // properties, patternProperties
        m_properties = other.m_properties;
        m_patternProperties = other.m_patternProperties;
        m_additionalPropertiesIsFalse = other.m_additionalPropertiesIsFalse;
        m_additionalPropertiesSchema.reset(
            other.m_additionalPropertiesSchema ? new SwJsonSchema(*other.m_additionalPropertiesSchema) : nullptr);

        m_required = other.m_required;
        m_dependentRequired = other.m_dependentRequired;
        m_allOf = other.m_allOf;
        m_anyOf = other.m_anyOf;
        m_oneOf = other.m_oneOf;

        m_notSchema.reset(other.m_notSchema ? new SwJsonSchema(*other.m_notSchema) : nullptr);
        m_ifSchema.reset(other.m_ifSchema ? new SwJsonSchema(*other.m_ifSchema) : nullptr);
        m_thenSchema.reset(other.m_thenSchema ? new SwJsonSchema(*other.m_thenSchema) : nullptr);
        m_elseSchema.reset(other.m_elseSchema ? new SwJsonSchema(*other.m_elseSchema) : nullptr);

        m_defs = other.m_defs;
        m_isValide = other.m_isValide;
        m_parent = other.m_parent;
        m_recursiveSchema = other.m_recursiveSchema;
        m_internalCustomKeywordValidator = other.m_internalCustomKeywordValidator;
    }

    void deduceTypeFromConstraints()
    {
        bool mightBeString = (m_minLength >= 0 || m_maxLength >= 0 || m_hasPattern || !m_format.isEmpty());
        bool mightBeNumber = (m_hasMinimum || m_hasMaximum || m_hasMultipleOf);
        bool mightBeObject = (!m_properties.isEmpty() || !m_patternProperties.isEmpty() || !m_required.isEmpty() ||
                              m_additionalPropertiesSchema || m_additionalPropertiesIsFalse);
        bool mightBeArray = (!m_prefixItemsSchemas.isEmpty() || m_itemsSchema || (m_minItems >= 0) || (m_maxItems >= 0));

        int count = 0;
        if (mightBeString)
            count++;
        if (mightBeNumber)
            count++;
        if (mightBeObject)
            count++;
        if (mightBeArray)
            count++;

        if (count == 1) {
            if (mightBeString)
                m_type = SchemaType::String;
            else if (mightBeNumber)
                m_type = SchemaType::Number;
            else if (mightBeObject)
                m_type = SchemaType::Object;
            else if (mightBeArray)
                m_type = SchemaType::Array;
        }
    }

    bool checkType(const QJsonValue &v) const
    {
        switch (m_type) {
            case SchemaType::String:
                return v.isString();
            case SchemaType::Number:
                return v.isDouble();
            case SchemaType::Integer:
                if (v.isDouble()) {
                    double value = v.toDouble();
                    return std::floor(value) == value;
                }
                return false;
            case SchemaType::Boolean:
                return v.isBool();
            case SchemaType::Object:
                return v.isObject();
            case SchemaType::Array:
                return v.isArray();
            case SchemaType::Null:
                return v.isNull();
            case SchemaType::Invalid:
            default:
                return true;
        }
    }

    SchemaType getType(const QJsonValue &v) const
    {
        if (v.isString())
            return SchemaType::String;
        else if (v.isDouble())
            return SchemaType::Number;
        else if (v.isBool())
            return SchemaType::Boolean;
        else if (v.isObject())
            return SchemaType::Object;
        else if (v.isArray())
            return SchemaType::Array;
        else if (v.isNull())
            return SchemaType::Null;
        else
            return SchemaType::Invalid; // Cas par défaut pour un type non reconnu
    }

    static bool checkFormat(const QString &value, const QString &formatName, QString *errorMessage)
    {
        // Implémentez ici la logique pour "email", "date-time", etc.
        // Ex en simplifié :
        // -- "email" --
        if (formatName == "email") {
            static const QRegularExpression emailRe(R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)");
            if (!emailRe.match(value).hasMatch())
                return setError(errorMessage, "Le format email est invalide.");

            // -- "date-time" (RFC3339 simplifié) --
        }
        else if (formatName == "date-time") {
            static const QRegularExpression dateTimeRe(
                R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})T(?<hour>\d{2}):(?<min>\d{2}):(?<sec>\d{2})(\.\d+)?(Z|[+\-]\d{2}:\d{2})$)");
            if (!dateTimeRe.match(value).hasMatch())
                return setError(errorMessage, "Le format date-time (RFC3339) est invalide.");

            // -- "date" (YYYY-MM-DD) --
        }
        else if (formatName == "date") {
            static const QRegularExpression dateRe(R"(^(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})$)");
            if (!dateRe.match(value).hasMatch())
                return setError(errorMessage, "Le format date (YYYY-MM-DD) est invalide.");

            // -- "time" (hh:mm:ss(.fraction)?(Z|±hh:mm)?) --
        }
        else if (formatName == "time") {
            static const QRegularExpression timeRe(
                R"(^(?<hour>\d{2}):(?<min>\d{2})(:(?<sec>\d{2})(\.\d+)?)?(Z|[+\-]\d{2}:\d{2})?$)");
            if (!timeRe.match(value).hasMatch())
                return setError(errorMessage, "Le format time est invalide.");

            // -- "hostname" (RFC1123 simplifié) --
        }
        else if (formatName == "hostname") {
            static const QRegularExpression hostnameRe(
                R"(^(?=.{1,253}$)([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)(\.([a-zA-Z0-9](?:[a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?))*$)");
            if (!hostnameRe.match(value).hasMatch())
                return setError(errorMessage, "Le format hostname est invalide.");

            // -- "ipv4" --
        }
        else if (formatName == "ipv4") {
            static const QRegularExpression ipv4Re(
                R"(^(25[0-5]|2[0-4]\d|[01]?\d?\d)\."
                  R"(25[0-5]|2[0-4]\d|[01]?\d?\d)\."
                  R"(25[0-5]|2[0-4]\d|[01]?\d?\d)\."
                  R"(25[0-5]|2[0-4]\d|[01]?\d?\d)$)");
            if (!ipv4Re.match(value).hasMatch())
                return setError(errorMessage, "Le format IPv4 est invalide.");

            // -- "ipv6" (simplifié, incluant '::') --
        }
        else if (formatName == "ipv6") {
            // Version simplifiée. Pour un check complet, se fier à QHostAddress ou RFC 4291.
            static const QRegularExpression ipv6Re(
                // Ce pattern essaie de capturer divers cas "compressés" via '::'.
                R"((^(([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|)"
                R"(^(::([0-9A-Fa-f]{1,4}:){0,5}((([0-9A-Fa-f]{1,4}))|:))$)");
            if (!ipv6Re.match(value).hasMatch())
                return setError(errorMessage, "Le format IPv6 est invalide.");

            // -- "uri" (simplifié) --
        }
        else if (formatName == "uri") {
            // On peut aussi faire `QUrl url(value); if (!url.isValid()) ...`
            // Pour un check plus élaboré sur tous les aspects d'une URI (RFC 3986).
            static const QRegularExpression uriRe(R"(^[A-Za-z][A-Za-z0-9+\-.]*:)"
                                                  R"(\/\/?([^\s/]+)(\/\S*)?$)");
            if (!uriRe.match(value).hasMatch())
                return setError(errorMessage, "Le format URI est invalide.");

            // -- "uuid" (RFC 4122 version 4 / standard) --
        }
        else if (formatName == "uuid") {
            // Forme : 8-4-4-4-12 hexadécimal
            static const QRegularExpression uuidRe(
                R"(^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$)");
            if (!uuidRe.match(value).hasMatch())
                return setError(errorMessage, "Le format uuid est invalide.");

            // -- "phone" (exemple très général) --
        }
        else if (formatName == "phone") {
            // Ici on autorise : +, (), espace, tiret, chiffres, etc.
            // À affiner selon le plan de numérotation international, etc.
            static const QRegularExpression phoneRe(R"(^[+\-()0-9\s]+$)" // très permissif
            );
            if (!phoneRe.match(value).hasMatch())
                return setError(errorMessage, "Le format phone est invalide.");

            // -- "credit-card" : check via Regex + Luhn --
        }
        else if (formatName == "credit-card") {
            // Regex simple : 13 à 19 chiffres
            static const QRegularExpression ccRe(R"(^\d{13,19}$)");
            if (!ccRe.match(value).hasMatch())
                return setError(errorMessage, "Le format credit-card est invalide (doit être 13-19 chiffres).");
            // Vérification Luhn
            if (!luhnCheck(value))
                return setError(errorMessage, "Le numéro de carte de crédit n'est pas valide (Luhn).");
        }
        return true;
    }

    static bool luhnCheck(const QString &ccNumberInput)
    {
        QString ccNumber = ccNumberInput;
        ccNumber.remove(' ');
        ccNumber.remove('-');
        static const QRegularExpression reDigits("^[0-9]+$");
        if (!reDigits.match(ccNumber).hasMatch())
            return false;
        if (ccNumber.size() < 13 || ccNumber.size() > 19)
            return false;

        int sum = 0;
        bool alternate = false;
        for (int i = ccNumber.size() - 1; i >= 0; --i) {
            int n = ccNumber.at(i).digitValue();
            if (alternate) {
                n *= 2;
                if (n > 9)
                    n = (n % 10) + 1;
            }
            sum += n;
            alternate = !alternate;
        }
        return (sum % 10) == 0;
    }

    static bool setError(QString *errorMessage, const QString &msg)
    {
        if (errorMessage)
            *errorMessage = msg;
        return false;
    }

    static QString toString(SchemaType t)
    {
        switch (t) {
            case SchemaType::String:
                return "string";
            case SchemaType::Number:
                return "number";
            case SchemaType::Integer:
                return "integer";
            case SchemaType::Boolean:
                return "boolean";
            case SchemaType::Object:
                return "object";
            case SchemaType::Array:
                return "array";
            case SchemaType::Null:
                return "null";
            default:
                return "invalid";
        }
    }

    static QString toString(const QJsonValue &v)
    {
        if (v.isString())
            return "string";
        if (v.isBool())
            return "boolean";
        if (v.isObject())
            return "object";
        if (v.isArray())
            return "array";
        if (v.isNull())
            return "null";
        if (v.isDouble()) {
            double value = v.toDouble();
            if (std::floor(value) == value)
                return "integer";
            return "number";
        }
        return "undefined";
    }

    SwJsonSchema::SchemaType parseType(const QJsonValue &val) const
    {
        if (!val.isString())
            return SchemaType::Invalid;
        QString typeStr = val.toString().toLower().trimmed();
        if (typeStr == "string")
            return SchemaType::String;
        if (typeStr == "number")
            return SchemaType::Number;
        if (typeStr == "integer")
            return SchemaType::Integer;
        if (typeStr == "boolean")
            return SchemaType::Boolean;
        if (typeStr == "object")
            return SchemaType::Object;
        if (typeStr == "array")
            return SchemaType::Array;
        if (typeStr == "null")
            return SchemaType::Null;
        return SchemaType::Invalid;
    }

    inline QString resolveUri(const SwJsonSchema *parent, const QString &id)
    {
        if (!parent || parent->m_baseUri.isEmpty())
            return id.trimmed();

        QUrl baseUrl(parent->m_baseUri);

        QUrl relativeUrl(id.trimmed());

        QUrl resolved = baseUrl.resolved(relativeUrl);

        return resolved.toString();
    }

    static SwJsonSchemaRegistry *getRegistry(const QString baseUri)
    {
        static QMap<QString, SwJsonSchemaRegistry *> registryBook;
        if (!registryBook.contains(baseUri.toLower()))
            registryBook.insert(baseUri.toLower(), new SwJsonSchemaRegistry());
        return registryBook.value(baseUri.toLower());
    }

    SwJsonSchema *parent() const
    {
        return m_parent;
    }

    SwJsonSchema *findMainSchema()
    {
        SwJsonSchema *seeked = this;
        while (seeked->parent())
            seeked = seeked->parent();
        return seeked;
    }

    static QMap<QString, Validator> &getCustomKeywordRegistry()
    {
        static QMap<QString, Validator> customKeywordRegistry;
        return customKeywordRegistry;
    }

private:
    // -----------------------------------------------------------------------
    //                      Données membres
    // -----------------------------------------------------------------------
    QString m_dollarSchema;
    QString m_baseUri;
    QString m_dollarAnchor;
    QString m_dollarRef;

    SchemaType m_type = SchemaType::Invalid;
    QList<QJsonValue> m_enumValues;
    QJsonValue m_constValue = QJsonValue(QJsonValue::Undefined);

    // Numérique
    double m_multipleOf = 0.0;
    bool m_hasMultipleOf = false;
    double m_minimum = 0.0;
    double m_maximum = 0.0;
    bool m_exclusiveMinimum = false;
    bool m_exclusiveMaximum = false;
    bool m_hasMinimum = false;
    bool m_hasMaximum = false;

    // String
    int m_minLength = -1;
    int m_maxLength = -1;
    QString m_pattern;
    bool m_hasPattern = false;
    QString m_format;

    // Array
    int m_minItems = -1;
    int m_maxItems = -1;
    bool m_uniqueItems = false;
    QScopedPointer<SwJsonSchema> m_itemsSchema;
    QList<SwJsonSchema *> m_prefixItemsSchemas;
    QScopedPointer<SwJsonSchema> m_additionalItemsSchema;
    SwJsonSchema *m_recursiveSchema = nullptr;

    QScopedPointer<SwJsonSchema> m_containsSchema;
    int m_minContains = -1;
    int m_maxContains = -1;

    // Object
    QMap<QString, SwJsonSchema> m_properties;
    QMap<QString, SwJsonSchema> m_patternProperties;
    bool m_additionalPropertiesIsFalse = false;
    QScopedPointer<SwJsonSchema> m_additionalPropertiesSchema;
    QSet<QString> m_required;
    QMap<QString, QStringList> m_dependentRequired;

    // Combinaisons logiques
    QList<SwJsonSchema> m_allOf;
    QList<SwJsonSchema> m_anyOf;
    QList<SwJsonSchema> m_oneOf;
    QScopedPointer<SwJsonSchema> m_notSchema;

    // if/then/else
    QScopedPointer<SwJsonSchema> m_ifSchema;
    QScopedPointer<SwJsonSchema> m_thenSchema;
    QScopedPointer<SwJsonSchema> m_elseSchema;

    // $defs
    QMap<QString, SwJsonSchema> m_defs;

    // if/then/else : on stocke les noms de propriétés validées
    mutable QSet<QString> m_ifThenElsePropertyValidated;

    bool m_isValide = false;
    SwJsonSchema *m_parent;
    QList<KeywordJsonValidator> m_internalCustomKeywordValidator;
};

#endif // SWJSONSCHEMA_H
