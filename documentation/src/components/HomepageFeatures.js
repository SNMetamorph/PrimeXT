import React from 'react';
import clsx from 'clsx';
import styles from './HomepageFeatures.module.css';

const FeatureList = [
  {
    title: 'Open Source',
    Svg: require('../../static/img/osi_standard_logo.png').default,
    description: (
      <>
        PrimeXT is open source project, this means everyone from community can take part in development. 
        Also it's free and available for all.
      </>
    ),
  },
  {
    title: 'Most Functional Toolkit',
    Svg: require('../../static/img/terminal_logo.png').default,
    description: (
      <>
        It provides a lot of features that other mods haven't. 
        Like advanced graphics features, extended limits, new entities, rigid body physics. 
      </>
    ),
  },
  {
    title: 'Crossplatform',
    Svg: require('../../static/img/crossplatform_logo.png').default,
    description: (
      <>
        You can develop your mod not only for one specific platform.
        Now available Windows and Linux, in future we plans to support Android also.
      </>
    ),
  },
];

function Feature({Svg, title, description}) {
  return (
    <div className={clsx('col col--4')}>
      <div className="text--center">
        <Svg className={styles.featureSvg} alt={title} />
      </div>
      <div className="text--center padding-horiz--md">
        <h3>{title}</h3>
        <p>{description}</p>
      </div>
    </div>
  );
}

export default function HomepageFeatures() {
  return (
    <section className={styles.features}>
      <div className="container">
        <div className="row">
          {FeatureList.map((props, idx) => (
            <Feature key={idx} {...props} />
          ))}
        </div>
      </div>
    </section>
  );
}
