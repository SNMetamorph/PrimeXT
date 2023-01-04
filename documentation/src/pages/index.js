import React from 'react';
import clsx from 'clsx';
import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import styles from './index.module.css';
import HomepageFeatures from '../components/HomepageFeatures';

function HomepageHeader() {
  const {siteConfig} = useDocusaurusContext();
  return (
    <header className={clsx('hero hero--primary', styles.heroBanner)}>
      <div className="container">
        <h1 className="hero__title">{siteConfig.title}</h1>
        <p className="hero__subtitle">{siteConfig.tagline}</p>
        <div className={styles.buttons}>
          <Link
            className={"button button--secondary button--lg " + styles.buttonIntro}
            to="/docs/rus/intro">
            🇷🇺 Читать вступление
          </Link>
          <Link
            className={"button button--secondary button--lg " + styles.buttonIntro}
            to="/docs/eng/intro">
            🇬🇧 Read Intro
          </Link>
        </div>
      </div>
    </header>
  );
}

export default function Home() {
  const {siteConfig} = useDocusaurusContext();
  return (
    <Layout
      title={`Main Page`}
      description="Modern SDK for the Xash3D FWGS engine, with cross-platform support and improved graphics/physics, while retaining all the features and approaches to work inherent in GoldSrc and Xash3D">
      <HomepageHeader />
      <main>
        <HomepageFeatures />
      </main>
    </Layout>
  );
}
