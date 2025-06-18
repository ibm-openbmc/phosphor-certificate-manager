# Acf Shell

A shell environment for running shell scripts embedded in targeted ACF files.
This allows dynamic execution of scripts as part of the ACF workflow, enabling
custom script running using targeted acf.

## Design

```mermaid
---
config:
  layout: dagre
---
flowchart TD
 subgraph s1["acf-manager"]
    direction TB
        D{"Validate and Extract Metadata"}
        C["acf-manager Service"]
        E["Return Error to bmcweb"]
        F["Extract Metadata"]
        G{"Identify ACF Type"}
        H["Resource Dump Handler"]
        I["bmcshell Handler"]
        J["Service Handler"]
        K["Admin Reset Handler"]
        L["acfshell Service"]
  end
 subgraph acfshell["acfshell"]
    direction TB
        M["Shell object"]
        subgraph async["Async Script Execution"]
          direction TB
            N["Script Object"]
            O["Script Runner"]
            P["Timer Task"]
            Q["Finished Script"]
            R{"Dump Needed"}
            S["Clear Results"]
            T["Execute Dump"]
        end
  end
    A["Redfish Client"] -- Installs ACF file --> B["bmcweb"]
    B -- Invokes --> C
    C --> D
    D -- Validation Failed --> E
    D -- Validation Success --> F
    F --> G
    G -- Resource Dump --> H
    G -- bmcshell --> I
    G -- Service --> J
    G -- Admin Reset --> K
    I -- Invokes --> L
    L --> M
    M -- Creates --> N
    N -- Assigns shell Task  --> O
    N -- Starts Timer  --> P
    P -- Timeout --> O
    O --> Q
    Q --> R
    R -- Yes --> T
    T -- Finished --> S
    R -- No --> S

```
