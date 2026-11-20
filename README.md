# doqe

**One platform to discover, query, manage, and govern Apache Iceberg data.**

## Vision

Build a fully open-source, Iceberg-native data platform that brings together a distributed SQL query engine, a self-built data catalog, and data management capabilities into one cohesive system. Today, building a data lakehouse often requires integrating multiple independent components for querying, metadata management, governance, and storage. doqe aims to simplify that experience with a unified platform designed specifically for Apache Iceberg.

The owner has chosen C++ as the implementation language.

## Goals

The platform will enable developers and data engineers to:

- **Discover data** — Explore tables, schemas, snapshots, metadata, and lineage through an integrated catalog.
- **Query data** — Execute SQL directly against Iceberg tables using a high-performance C++ execution engine.
- **Manage data** — Support inserts, updates, deletes, and reliable Iceberg transactions.
- **Scale computation** — Distribute query execution across multiple workers as workloads grow.
- **Govern data** — Manage ownership, access policies, audit history, and data lineage in one place.
- **Own their infrastructure** — Run on their own infrastructure using AWS S3 or S3-compatible object storage such as MinIO and Ceph.

## Guiding principles

- **Simplicity** — Make the platform easy to deploy, operate, and understand.
- **Iceberg-first** — Design the engine and catalog specifically around Apache Iceberg.
- **Performance** — Use C++ and columnar, vectorized execution to build an efficient foundation for analytical workloads.
- **Openness** — Build an open-source product with transparent architecture and community contributions.
- **Modularity** — Keep the components independently evolvable while providing a unified experience.

## Non-goals

Draft for the owner to review.

- Support table formats such as Delta Lake and Apache Hudi.
- Offer a proprietary or managed service or depend on a proprietary managed platform.
- Build distributed execution before the single-node engine works.

## Long-term ambition

Create a platform that starts as a lightweight, single-node Iceberg engine and evolves into a production-grade distributed lakehouse system. Make working with Apache Iceberg feel like working with one integrated database platform while retaining the openness, flexibility, and scalability of a data lakehouse.

## Docs

See [the documentation index](docs/README.md) and [agent instructions](AGENTS.md).
